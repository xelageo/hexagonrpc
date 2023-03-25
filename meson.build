#
# Build system root
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

project('sensh', 'c')

libqrtr = meson.get_compiler('c').find_library('qrtr')
libprotobuf_c = dependency('libprotobuf-c')

subdir('protobuf')
subdir('qmi')

executable('sensh',
  'sensh.c',
  qmi,
  protobufs,
  dependencies : [libprotobuf_c, libqrtr])
