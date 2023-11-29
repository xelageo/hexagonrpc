#include <errno.h>
#include <string.h>
#include "qmi_sns_client.h"

struct qmi_elem_info sns_client_req_msg_v01_ei[] = {
	{
		.data_type = QMI_DATA_LEN,
		.elem_len = 1,
		.elem_size = sizeof(uint16_t),
		.tlv_type = 1,
		.offset = offsetof(struct sns_client_req_msg_v01, payload_len),
	},
	{
		.data_type = QMI_UNSIGNED_1_BYTE,
		.elem_len = 65535,
		.elem_size = sizeof(uint8_t),
		.array_type = VAR_LEN_ARRAY,
		.tlv_type = 1,
		.offset = offsetof(struct sns_client_req_msg_v01, payload),
	},
	{
		.data_type = QMI_UNSIGNED_1_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint8_t),
		.tlv_type = 16,
		.offset = offsetof(struct sns_client_req_msg_v01, unkfield_1),
	},
	{}
};

struct qmi_elem_info sns_client_resp_msg_v01_ei[] = {
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint32_t),
		.tlv_type = 2,
		.offset = offsetof(struct sns_client_resp_msg_v01, unkfield_2),
	},
	{
		.data_type = QMI_UNSIGNED_8_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint64_t),
		.tlv_type = 16,
		.offset = offsetof(struct sns_client_resp_msg_v01, client_id),
	},
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint32_t),
		.tlv_type = 17,
		.offset = offsetof(struct sns_client_resp_msg_v01, result),
	},
	{}
};

struct qmi_elem_info sns_client_report_ind_msg_v01_ei[] = {
	{
		.data_type = QMI_UNSIGNED_8_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint64_t),
		.tlv_type = 1,
		.offset = offsetof(struct sns_client_report_ind_msg_v01, client_id),
	},
	{
		.data_type = QMI_DATA_LEN,
		.elem_len = 1,
		.elem_size = sizeof(uint16_t),
		.tlv_type = 2,
		.offset = offsetof(struct sns_client_report_ind_msg_v01, payload_len),
	},
	{
		.data_type = QMI_UNSIGNED_1_BYTE,
		.elem_len = 65535,
		.elem_size = sizeof(uint8_t),
		.array_type = VAR_LEN_ARRAY,
		.tlv_type = 2,
		.offset = offsetof(struct sns_client_report_ind_msg_v01, payload),
	},
	{}
};

