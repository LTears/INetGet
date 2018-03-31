##############################################################################
# INetGet - Lightweight command-line front-end to WinINet API
# Copyright (C) 2018 LoRd_MuldeR <MuldeR2@GMX.de>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# See https:#www.gnu.org/licenses/gpl-2.0-standalone.html for details!
##############################################################################

import sys
import re
import math

from subprocess import Popen, PIPE, CREATE_NEW_CONSOLE
from os import devnull
from shutil import copyfileobj
from time import sleep

# Parameters
ADDRESS = 'http://cdimage.ubuntu.com/ubuntu-core/16/current/ubuntu-core-16-amd64.img.xz'
NCHUNKS = 5
OUTNAME = 'ubuntu-core-16-amd64.img.xz'


##############################################################################
# STEP #1: Determine the total file size
##############################################################################

sys.stdout.write('Determining file size, please wait...\n')

process = Popen(['INetGet.exe', '--verb=HEAD', ADDRESS, devnull], stderr=PIPE)
stdout, stderr = process.communicate()

if not process.returncode == 0:
    sys.stdout.write('ERROR: Failed to determine file size!\n\n')
    sys.stdout.write(stderr.decode("utf-8"))
    sys.exit(-1)

match = re.search(r"Content\s+length\s*:\s*(\d+)\s*Byte", stderr.decode("utf-8"), re.IGNORECASE)

if not match:
    sys.stdout.write('\nERROR: Failed to determine file size!\n\n')
    sys.stdout.write(stderr.decode("utf-8"))
    sys.exit(-1)

sys.stdout.write('Done.\n\n')

size_total = int(match.group(1))
sys.stdout.write('Total file size is: %d Byte\n\n' % size_total)

if size_total < 1:
    sys.stdout.write('\nERROR: File appears to be empty!\n\n')
    sys.exit(-1)


##############################################################################
# STEP #2: Start the download processes
##############################################################################

size_chunk = size_total // NCHUNKS
size_rmndr = math.fmod(size_total, size_chunk)

while size_rmndr >= NCHUNKS:
    size_chunk = size_chunk + (size_rmndr // NCHUNKS)
    size_rmndr = math.fmod(size_total, size_chunk)

sys.stdout.write('Chunksize: %d\n'   % size_chunk)
sys.stdout.write('Remainder: %d\n\n' % size_rmndr)

offset = 0
file_no = 0
proc_list = []

if size_chunk > 0:
    for i in range(0, NCHUNKS):
        range_end = offset + size_chunk - 1 + (0 if (i < NCHUNKS-1) else size_rmndr) #add remainder to *last* chunk!
        sys.stdout.write('Chunk #%d: %d - %d\n' % (file_no, offset, range_end))
        proc_list.append(Popen(['INetGet.exe', '--range-off=%d' % offset, '--range-end=%d' % range_end, ADDRESS, OUTNAME+"~%d" % file_no], creationflags=CREATE_NEW_CONSOLE))
        offset, file_no = offset + size_chunk, file_no + 1
        sleep(1)

sys.stdout.write('\nDownloads are running, please be patient...\n')


##############################################################################
# STEP #3: Wait for completion...
##############################################################################

proc_id = 0
success = True

while len(proc_list) > 0:
    subproc = proc_list.pop(0)
    subproc.communicate()
    if subproc.returncode == 0:
        sys.stdout.write('Chunk #%d succeeded.\n' % proc_id)
    else:
        sys.stdout.write('Chunk #%d failed !!!\n' % proc_id)
        success = False
    proc_id = proc_id + 1

if success:
    sys.stdout.write('Completed.\n\n')
else:
    sys.stdout.write('Failed !!!\n\n')
    sys.exit(-1)


##############################################################################
# STEP #4: Concatenate chunk files
##############################################################################

sys.stdout.write('Concatenating chunks, please wait...\n')

with open(OUTNAME, 'wb') as wfd:
    file_no = 0
    for i in range(0, NCHUNKS):
        with open(OUTNAME+"~%d" % file_no, 'rb') as fd:
            copyfileobj(fd, wfd)
            file_no = file_no + 1

sys.stdout.write('Done.\n\n')