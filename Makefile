#
# Build system
#
# Copyright (C) 2022 Richard Acayan
#
# This file is part of sensh.
#
# Sensh is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

QMIC = qmic
CC = gcc
LIBS = -lprotobuf-c -lqrtr
CFLAGS = -Wall -Wextra -Wpedantic -Ibuild -I.
PROTOC_C = protoc-c

build/sensh: build/sensh.o build/qmi_sns_client.o build/sns_client_event_msg.pb-c.o build/sns_client_request_msg.pb-c.o build/sns_proximity_event.pb-c.o build/sns_std_attr_event.pb-c.o build/sns_std_attr_req.pb-c.o build/sns_std_sensor_event.pb-c.o build/sns_std_suid.pb-c.o build/sns_suid_event.pb-c.o build/sns_suid_req.pb-c.o
	$(CC) -o build/sensh build/sensh.o build/qmi_sns_client.o build/sns_client_event_msg.pb-c.o build/sns_client_request_msg.pb-c.o build/sns_proximity_event.pb-c.o build/sns_std_attr_event.pb-c.o build/sns_std_attr_req.pb-c.o build/sns_std_sensor_event.pb-c.o build/sns_std_suid.pb-c.o build/sns_suid_event.pb-c.o build/sns_suid_req.pb-c.o $(LIBS)

build/qmi_%.d: build/qmi_%.c
	$(CC) $(CFLAGS) -MM -MT build/$*.o -o $@ $<

build/%.pb-c.d: build/%.pb-c.c
	$(CC) $(CFLAGS) -MM -MT build/$*.o -o $@ $<

build/%.d: %.c
	$(CC) $(CFLAGS) -MM -MT build/$*.o -o $@ $<

build/qmi_%.o: build/qmi_%.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.pb-c.o: build/%.pb-c.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/qmi_%.c build/qmi_%.h: qmi/%.qmi
	cd build && $(QMIC) -k < ../$<

build/%.pb-c.c build/%.pb-c.h: protobuf/%.proto
	$(PROTOC_C) --c_out=build -Iprotobuf $<

include build/sensh.d
include build/qmi_sns_client.d
include build/sns_client_event_msg.pb-c.d
include build/sns_client_request_msg.pb-c.d
include build/sns_proximity_event.pb-c.d
include build/sns_std_attr_event.pb-c.d
include build/sns_std_attr_req.pb-c.d
include build/sns_std_sensor_event.pb-c.d
include build/sns_std_suid.pb-c.d
include build/sns_suid_event.pb-c.d
include build/sns_suid_req.pb-c.d
