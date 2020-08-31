# Copyright 2020 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

# A class to use to send HTTP requests. Can be changed by 'set_engine_class'.
# Default is RequestsLibEngine.
_request_engine_cls = None
