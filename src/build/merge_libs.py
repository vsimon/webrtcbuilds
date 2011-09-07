#!/usr/bin/env python

# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Searches for libraries and/or object files on the specified path and
# merges them into a single library.

import subprocess
import sys

if __name__ == '__main__':
  if len(sys.argv) != 3:
    sys.stderr.write('Usage: ' + sys.argv[0] + ' <search_path> <output_lib>\n')
    sys.exit(2)

  search_path = sys.argv[1]
  output_lib = sys.argv[2]

  from subprocess import Popen, PIPE
  if sys.platform.startswith('linux'):
    Popen(["rm -f " + output_lib], shell=True)
    Popen(["rm -rf " + search_path + "/obj.target/*do_not_use*"], shell=True)
    Popen(["ar crs " + output_lib + " $(find " + search_path + " -name *\.o)"], 
        shell=True)

  elif sys.platform == 'darwin':
    Popen(["rm -f " + output_lib], shell=True)
    Popen(["rm -f " + search_path + "/*do_not_use*"], shell=True)
    Popen(["libtool -static -v -o " + output_lib + " $(find " + search_path + 
        " -name *\.a)"], shell=True)

  elif sys.platform == 'win32':
    # We need to execute a batch file to set some environment variables for the
    # lib command. VS 8 uses vsvars.bat and VS 9 uses vsvars32.bat. 
    # It's required that at least one of them is in the system PATH. 
    # We try both and suppress stderr and stdout to fail silently.
    Popen(["del " + output_lib], stderr=PIPE, stdout=PIPE, shell=True)
    Popen(["del /F /S /Q " + search_path + "/lib/*do_not_use*"], 
        shell=True)
    Popen(["vsvars.bat"], stderr=PIPE, stdout=PIPE, shell=True)
    Popen(["vsvars32.bat"], stderr=PIPE, stdout=PIPE, shell=True)
    Popen(["lib /OUT:" + output_lib + " " + search_path + "/lib/*.lib"], 
        shell=True)

  else:
    sys.stderr.write('Platform not supported: %r\n\n' % sys.platform)
    sys.exit(1)


sys.exit(0)
