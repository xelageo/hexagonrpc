#
# Protocol buffers build system
#
# Copyright (C) 2023 Richard Acayan
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

protoc = find_program('protoc-c')

protobuf_src = [
  'sns_client_event_msg',
  'sns_client_request_msg',
  'sns_proximity_event',
  'sns_std_attr_event',
  'sns_std_attr_req',
  'sns_std_sensor_event',
  'sns_std_suid',
  'sns_suid_event',
  'sns_suid_req']

protobufs = []

foreach protobuf : protobuf_src
  protobufs += custom_target(protobuf,
    command : [
      protoc,
      '--c_out=@OUTDIR@',
      '--proto_path=@CURRENT_SOURCE_DIR@',
      '--dependency_out=@DEPFILE@',
      '@INPUT@'],
    input : protobuf + '.proto',
    output : ['@BASENAME@.pb-c.c', '@BASENAME@.pb-c.h'],
    depfile : '@BASENAME@.pb-c.d')
endforeach
