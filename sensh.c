/*
 * Main interface logic
 *
 * Copyright (C) 2022 Richard Acayan
 *
 * This file is part of sensh.
 *
 * Sensh is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * This is a proof-of-concept for interacting with the Snapdragon Sensor Core
 * using the reverse-engineered protocol buffers. It was made to try to
 * initialize it, but the Pixel 3a is missing some sensors once the firmware is
 * loaded.
 *
 * Suspected initialization sequence on Android 9, Pixel 3a/3a XL
 * (pie-b4s4-release) branch:
 *  lookup resampler
 *  lookup accel_cal
 *  watch RETURNED_SUID
 *  lookup gyro_cal
 *  watch RETURNED_SUID
 *  lookup mag_cal
 *  watch RETURNED_SUID
 *  lookup remote_proc_state
 *  wakeup RETURNED_SUID
 *  lookup proximity
 *  watch RETURNED_SUID
 */

#include <errno.h>
#include <libqrtr.h>
#include <linux/qrtr.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "qmi_sns_client.h"

#include "sns_suid_req.pb-c.h"
#include "sns_suid_event.pb-c.h"
#include "sns_client_request_msg.pb-c.h"
#include "sns_client_event_msg.pb-c.h"
#include "sns_std_attr_req.pb-c.h"
#include "sns_std_attr_event.pb-c.h"

#define SHELL_POLL 0
#define QRTR_POLL 1

struct qrtr_client_ctx {
	int sock;
	int connected;
	uint32_t node;
	uint32_t port;

	uint16_t txn_id;

	// Input and input interpretation buffers
	size_t in_buf_len;
	void *in_buf;
	int max_argc;
	char **argv;

	// Output and output encoding buffers
	size_t payload_buf_len;
	void *payload_buf;
	size_t protomsg_buf_len;
	void *protomsg_buf;
	size_t out_buf_len;
	void *out_buf;
};

static SnsStdSuid lookup_suid = {
	PROTOBUF_C_MESSAGE_INIT(&sns_std_suid__descriptor),
	.suid_low = 0xABABABABABABABABUL,
	.suid_high = 0xABABABABABABABABUL,
};

void sns_handle_suid_event(struct qrtr_client_ctx *ctx, const void *payload, size_t payload_len);
void sns_handle_cal_event(struct qrtr_client_ctx *ctx, const void *payload, size_t payload_len);
void sns_handle_attr_event(struct qrtr_client_ctx *ctx, const void *payload, size_t payload_len);
void sns_handle_event(struct qrtr_client_ctx *ctx, const void *buf, size_t len);

void sns_qmi_send_request(struct qrtr_client_ctx *ctx, const void *protomsg, size_t protomsg_len) {
	int ret;

	struct sns_client_req_msg_v01 req = {
		.payload_len = protomsg_len,
	};

	struct qrtr_packet pkt = {
		.data = ctx->out_buf,
		.data_len = ctx->out_buf_len,
	};

	memcpy(req.payload, protomsg, protomsg_len);

	qmi_encode_message(&pkt, 0, SNS_CLIENT_REQ_V01, ctx->txn_id, &req, sns_client_req_msg_v01_ei);
	ret = qrtr_sendto(ctx->sock, ctx->node, ctx->port, pkt.data, pkt.data_len);
	if (ret == -1) {
		perror("sensh: could not send request");
		return;
	}

	ctx->txn_id++;
}

void sns_qmi_handle_message(struct qrtr_client_ctx *ctx, const struct qrtr_packet *pkt) {
	unsigned int msg_id;

	struct sns_client_resp_msg_v01 response;
	struct sns_client_report_ind_msg_v01 event;

	qmi_decode_header(pkt, &msg_id);

	if (msg_id == SNS_CLIENT_RESP_V01) {
		qmi_decode_message(&response, NULL, pkt, 2, msg_id, sns_client_resp_msg_v01_ei);
		if (response.result)
			fprintf(stderr, "sensh: received error %u\n", response.result);
	} else if (msg_id == SNS_CLIENT_IND_V01_SMALL
		|| msg_id == SNS_CLIENT_IND_V01_LARGE) {
		qmi_decode_message(&event, NULL, pkt, 4, msg_id, sns_client_report_ind_msg_v01_ei);
		sns_handle_event(ctx, event.payload, event.payload_len);
	} else {
		printf("sensh: received unhandled message\n");
	}
}

void sns_send_request(struct qrtr_client_ctx *ctx, SnsStdSuid *suid, uint32_t msg_id, void *payload, size_t payload_len) {
	SnsClientRequestMsg__PmConfig pm_config;
	SnsClientRequestMsg__Body body;
	SnsClientRequestMsg msg;
	int len;

	sns_client_request_msg__pm_config__init(&pm_config);
	pm_config.client_proc_type = 1;
	pm_config.delivery_type = 0;

	sns_client_request_msg__body__init(&body);
	body.has_is_passive = 1;
	body.is_passive = 0;
	if (payload != NULL) {
		body.has_payload = 1;
		body.payload.len = payload_len;
		body.payload.data = payload;
	}

	sns_client_request_msg__init(&msg);
	msg.suid = suid;
	msg.msg_id = msg_id;
	msg.susp_config = &pm_config;
	msg.request = &body;

	len = sns_client_request_msg__pack(&msg, ctx->protomsg_buf);

	sns_qmi_send_request(ctx, ctx->protomsg_buf, len);
}

void sns_parse_suid(const char *hex, SnsStdSuid *suid) {
	char *mid;

	suid->suid_high = strtoul(hex, &mid, 16);
	suid->suid_low = strtoul(&mid[1], NULL, 16);
}

void sns_handle_event(struct qrtr_client_ctx *ctx, const void *buf, size_t len) {
	SnsClientEventMsg *msg;
	size_t i;

	msg = sns_client_event_msg__unpack(NULL, len, buf);

	for (i = 0; i < msg->n_events; i++) {
		if (msg->events[i]->msg_id == 768) // SNS_SUID_MSGID_SNS_SUID_EVENT
			sns_handle_suid_event(ctx, msg->events[i]->payload.data, msg->events[i]->payload.len);
		else if (msg->events[i]->msg_id == 1022) // SNS_CAL_MSGID_SNS_CAL_EVENT
			sns_handle_cal_event(ctx, msg->events[i]->payload.data, msg->events[i]->payload.len);
		else if (msg->events[i]->msg_id == 128) // SNS_STD_MSGID_SNS_STD_ATTR_EVENT
			sns_handle_attr_event(ctx, msg->events[i]->payload.data, msg->events[i]->payload.len);
		else
			printf("sensh: received unhandled event %u\n", msg->events[i]->msg_id);
	}

	sns_client_event_msg__free_unpacked(msg, NULL);
}

void sns_send_suid_req(struct qrtr_client_ctx *ctx, const char *data_type) {
	SnsSuidReq payload;
	int len;

	sns_suid_req__init(&payload);
	payload.data_type = strdup(data_type);
	payload.unkfield_2 = 1;
	payload.unkfield_3 = 0;

	len = sns_suid_req__pack(&payload, ctx->payload_buf);

	free(payload.data_type);

	sns_send_request(ctx, &lookup_suid, 512, ctx->payload_buf, len);
}

void sns_send_watch_req(struct qrtr_client_ctx *ctx, SnsStdSuid *suid) {
	sns_send_request(ctx, suid, 514, NULL, 0);
}

void sns_send_attr_req(struct qrtr_client_ctx *ctx, SnsStdSuid *suid) {
	SnsStdAttrReq payload;
	int len;

	sns_std_attr_req__init(&payload);
	payload.unkfield_4 = 0;

	len = sns_std_attr_req__pack(&payload, ctx->payload_buf);

	sns_send_request(ctx, suid, 1, ctx->payload_buf, len);
}

void sns_handle_suid_event(struct qrtr_client_ctx *ctx, const void *payload, size_t payload_len) {
	SnsSuidEvent *event;

	event = sns_suid_event__unpack(NULL, payload_len, payload);

	if (event->suid != NULL)
		printf("sensh: %s sensor found: %016lXI%016lX\n", event->data_type, event->suid->suid_high, event->suid->suid_low);
	else
		printf("sensh: lookup found no %s sensor\n", event->data_type);

	sns_suid_event__free_unpacked(event, NULL);
}

void sns_handle_cal_event(struct qrtr_client_ctx *ctx, const void *payload, size_t payload_len) {
	printf("sensh: received cal message, raw: ");

	for (size_t i = 0; i < payload_len; i++)
		printf("%02X", ((const char *) payload)[i]);

	putchar('\n');
}

void sns_handle_attr_event(struct qrtr_client_ctx *ctx, const void *payload, size_t payload_len) {
	SnsStdAttrEvent *event;

	event = sns_std_attr_event__unpack(NULL, payload_len, payload);

	for (int i = 0; i < event->n_attrs; i++) {
		switch (event->attrs[i]->attr_id) {
			case 0:
				printf("sensh: name: ");
				break;
			case 1:
				printf("sensh: vendor: ");
				break;
			case 2:
				printf("sensh: type: ");
				break;
			case 3:
				printf("sensh: available: ");
				break;
			case 4:
				printf("sensh: version: ");
				break;
			case 5:
				printf("sensh: api: ");
				break;
			case 6:
				printf("sensh: rates: ");
				break;
			case 16:
				printf("sensh: stream type: ");
				break;
			case 21:
				printf("sensh: physical sensor: ");
				break;
			default:
				printf("sensh: unknown attr %u: ", event->attrs[i]->attr_id);
				break;
		}

		for (int j = 0; j < event->attrs[i]->value->n_values; j++) {
			if (event->attrs[i]->value->values[j]->subtype)
				printf("(subtype)");
			else if (event->attrs[i]->value->values[j]->str)
				printf("%s", event->attrs[i]->value->values[j]->str);
			else if (event->attrs[i]->value->values[j]->has_flt)
				printf("%f", event->attrs[i]->value->values[j]->flt);
			else if (event->attrs[i]->value->values[j]->has_sint)
				printf("%lu", event->attrs[i]->value->values[j]->sint);
			else if (event->attrs[i]->value->values[j]->has_boolean)
				printf("%u", event->attrs[i]->value->values[j]->boolean);

			if (j + 1 < event->attrs[i]->value->n_values)
				printf(", ");
		}

		putchar('\n');
	}

	sns_std_attr_event__free_unpacked(event, NULL);
}

static void run_cmd_lookup(struct qrtr_client_ctx *ctx, int argc, char **argv) {
	int i;

	if (argc < 2) {
		sns_send_suid_req(ctx, "");
	} else {
		for (i = 1; i < argc; i++)
			sns_send_suid_req(ctx, argv[i]);
	}
}

static void run_cmd_watch(struct qrtr_client_ctx *ctx, int argc, char **argv) {
	SnsStdSuid suid;
	int i;

	if (argc < 2) {
		fprintf(stderr, "sensh: usage: watch SUID ...\n");
		return;
	}

	sns_std_suid__init(&suid);

	for (i = 0; i < argc; i++) {
		sns_parse_suid(argv[i], &suid);
		sns_send_watch_req(ctx, &suid);
	}
}

static void run_cmd_attr(struct qrtr_client_ctx *ctx, int argc, char **argv) {
	SnsStdSuid suid;
	int i;

	if (argc < 2) {
		fprintf(stderr, "sensh: usage: attr SUID ...\n");
		return;
	}

	sns_std_suid__init(&suid);

	for (i = 0; i < argc; i++) {
		sns_parse_suid(argv[i], &suid);
		sns_send_attr_req(ctx, &suid);
	}
}

static int run_shell_command(struct qrtr_client_ctx *ctx) {
	int len;
	int argc = 0;
	char *argend;

	len = read(STDIN_FILENO, ctx->in_buf, ctx->in_buf_len);
	if (len == -1) {
		perror("sensh: could not read from stdin");
		return 1;
	} else if (len == 0)
		return 1;

	ctx->argv[0] = ctx->in_buf;
	argc++;

	argend = memchr(ctx->argv[argc - 1], ' ', len);
	if (argend == NULL)
	       argend = memchr(ctx->argv[argc - 1], '\n', len);
	if (argend != NULL)
		*argend = 0;

	while (argc < ctx->max_argc && len - strlen(ctx->argv[argc - 1]) - 1 > 0) {
		len -= strlen(ctx->argv[argc - 1]) + 1;
		ctx->argv[argc] = ctx->argv[argc - 1] + strlen(ctx->argv[argc - 1]) + 1;
		argc++;

		argend = memchr(ctx->argv[argc - 1], ' ', len);
		if (argend == NULL)
		       argend = memchr(ctx->argv[argc - 1], '\n', len);
		if (argend != NULL)
			*argend = 0;
	}

	if (!strcmp(ctx->argv[0], "lookup"))
		run_cmd_lookup(ctx, argc, ctx->argv);
	else if (!strcmp(ctx->argv[0], "watch"))
		run_cmd_watch(ctx, argc, ctx->argv);
	else if (!strcmp(ctx->argv[0], "attr"))
		run_cmd_attr(ctx, argc, ctx->argv);
	else
		fprintf(stderr, "sensh: invalid command\n");

	return 0;
}

static void handle_sensor_message(struct qrtr_client_ctx *ctx) {
	int len;
	struct qrtr_packet pkt;
	struct sockaddr_qrtr sockaddr;
	socklen_t socklen = sizeof(sockaddr);

	len = recvfrom(ctx->sock, ctx->in_buf, ctx->in_buf_len, 0, (void *) &sockaddr, &socklen);
	if (len == -1) {
		perror("sensh: could not read sensor message");
		return;
	}

	qrtr_decode(&pkt, ctx->in_buf, len, &sockaddr);

	if (pkt.type == QRTR_TYPE_DEL_SERVER) {
		printf("sensh: sensor core left\n");

		ctx->connected = 0;
	} else if (pkt.type == QRTR_TYPE_NEW_SERVER) {
		printf("sensh: sensor core found\n");

		ctx->node = pkt.node;
		ctx->port = pkt.port;
		ctx->connected = 1;
		ctx->txn_id = 1;
	} else if (pkt.type == QRTR_TYPE_DATA) {
		sns_qmi_handle_message(ctx, &pkt);
	} else {
		fprintf(stderr, "sensh: received unhandled message with qrtr type %d\n", pkt.type);
	}
}

void sensh_event_loop(int sock) {
	int ret;

	struct qrtr_client_ctx ctx = {
		.sock = sock,
		.connected = 0,
		.txn_id = 1,
	};

	struct pollfd pollfds[2] = {
		[SHELL_POLL] = {
			.fd = STDIN_FILENO,
			.events = POLLIN,
		},
		[QRTR_POLL] = {
			.fd = sock,
			.events = POLLIN,
			.revents = 0,
		},
	};

	ctx.in_buf_len = 65536;
	ctx.in_buf = malloc(ctx.in_buf_len);
	if (ctx.in_buf == NULL) {
		perror("sensh: could not allocate input buffer");
		return;
	}

	ctx.max_argc = 256; // DO NOT SET TO ZERO
	ctx.argv = malloc(sizeof(*ctx.argv) * ctx.max_argc);
	if (ctx.argv == NULL) {
		perror("sensh: could not allocate argument buffer");
		return;
	}

	ctx.payload_buf_len = 65536;
	ctx.payload_buf = malloc(ctx.payload_buf_len);
	if (ctx.payload_buf == NULL) {
		perror("sensh: could not allocate output buffer");
		return;
	}

	ctx.protomsg_buf_len = 65536;
	ctx.protomsg_buf = malloc(ctx.protomsg_buf_len);
	if (ctx.protomsg_buf == NULL) {
		perror("sensh: could not allocate output buffer");
		return;
	}

	ctx.out_buf_len = 65536;
	ctx.out_buf = malloc(ctx.out_buf_len);
	if (ctx.out_buf == NULL) {
		perror("sensh: could not allocate output buffer");
		return;
	}

	// Directly poll because we also want to listen for stdin
	while (poll(pollfds, 2, -1) != -1) {
		if (pollfds[SHELL_POLL].revents
		 && ctx.connected) {
			ret = run_shell_command(&ctx);
			if (ret)
				break;
		}

		if (pollfds[QRTR_POLL].revents)
			handle_sensor_message(&ctx);
	}

	free(ctx.in_buf);
	free(ctx.argv);

	free(ctx.payload_buf);
	free(ctx.protomsg_buf);
	free(ctx.out_buf);
}

int main() {
	int fd;
	int ret;

	fd = qrtr_open(0);
	if (fd == -1) {
		perror("sensh: could not connect to qrtr");
		return 1;
	}

	ret = qrtr_new_lookup(fd, 400, 0, 0);
	if (ret == -1) {
		perror("sensh: could not write to qrtr control port");
		return 1;
	}

	sensh_event_loop(fd);

	return 0;
}
