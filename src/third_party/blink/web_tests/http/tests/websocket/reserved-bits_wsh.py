import re
from mod_pywebsocket import common
from mod_pywebsocket import stream
from mod_pywebsocket.extensions import DeflateFrameExtensionProcessor

bit = 0


def _get_deflate_frame_extension_processor(request):
    for extension_processor in request.ws_extension_processors:
        if isinstance(extension_processor, DeflateFrameExtensionProcessor):
            return extension_processor
    return None


def web_socket_do_extra_handshake(request):
    match = re.search(r'\?compressed=(true|false)&bitNumber=(\d)$',
                      request.ws_resource)
    if match is None:
        msgutil.send_message(request,
                             'FAIL: Query value is incorrect or missing')
        return

    global bit
    compressed = match.group(1)
    bit = int(match.group(2))
    if compressed == 'false':
        request.ws_extension_processors = []  # using no extension response
    else:
        processor = _get_deflate_frame_extension_processor(request)
        if processor is None:
            request.ws_extension_processors = []  # using no extension response
        else:
            request.ws_extension_processors = [processor]  # avoid conflict


def web_socket_transfer_data(request):
    text = 'This message should be ignored.'
    opcode = common.OPCODE_TEXT
    if bit == 1:
        frame = stream.create_header(opcode, len(text), 1, 1, 0, 0, 0) + text
    elif bit == 2:
        frame = stream.create_header(opcode, len(text), 1, 0, 1, 0, 0) + text
    elif bit == 3:
        frame = stream.create_header(opcode, len(text), 1, 0, 0, 1, 0) + text
    else:
        frame = stream.create_text_frame('FAIL: Invalid bit number: %d' % bit)
    request.connection.write(frame)
