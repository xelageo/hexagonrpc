#ifndef __QMI_SNS_CLIENT_H__
#define __QMI_SNS_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>

#include "libqrtr.h"

#define SNS_CLIENT_REQ_V01 32
#define SNS_CLIENT_RESP_V01 32
#define SNS_CLIENT_IND_V01_SMALL 33
#define SNS_CLIENT_IND_V01_LARGE 34

struct sns_client_req_msg_v01 {
	uint32_t payload_len;
	uint8_t payload[65535];
	uint8_t unkfield_1;
};

struct sns_client_resp_msg_v01 {
	uint32_t unkfield_2;
	uint64_t client_id;
	uint32_t result;
};

struct sns_client_report_ind_msg_v01 {
	uint64_t client_id;
	uint32_t payload_len;
	uint8_t payload[65535];
};

extern struct qmi_elem_info sns_client_req_msg_v01_ei[];
extern struct qmi_elem_info sns_client_resp_msg_v01_ei[];
extern struct qmi_elem_info sns_client_report_ind_msg_v01_ei[];

#endif
