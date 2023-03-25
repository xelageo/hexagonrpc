#!/bin/sh

#
# QMI interface compiler wrapper
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

qmic="$1"

#
# Usage: qmic_wrapper.sh QMIC INPUT OUTPUT_DIRECTORY
#
# Example: qmic_wrapper.sh /usr/bin/qmic ../qmi/sns_client.qmi qmi/
#

#
# We need to move into the output directory so QMIC puts the files in the
# correct place, but the path to the input might be relative to the directory
# we are already in. Open the file now and pipe it to stdin so the pathname
# gets processed relative to the correct directory.
#
exec < "$2"

cd "$3"
exec "$qmic" -k
