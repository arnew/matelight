#!/usr/bin/env python

from socketserver import *
from time import time, strftime, sleep
from collections import namedtuple, deque
from itertools import product, cycle
import threading
import random

from ctypes import *

import numpy as np

from matelight import sendframe, DISPLAY_WIDTH, DISPLAY_HEIGHT, FRAME_SIZE


UDP_TIMEOUT = 3.0


class COLOR(Structure):
		_fields_ = [('r', c_uint8), ('g', c_uint8), ('b', c_uint8), ('a', c_uint8)]

class FRAMEBUFFER(Structure):
		_fields_ = [('data', POINTER(COLOR)), ('w', c_size_t), ('h', c_size_t)]

bdf = CDLL('./libbdf.so')
bdf.read_bdf_file.restype = c_void_p
bdf.framebuffer_render_text.restype = POINTER(FRAMEBUFFER)
bdf.framebuffer_render_text.argtypes= [c_char_p, c_void_p, c_void_p, c_size_t, c_size_t, c_size_t]

unifont = bdf.read_bdf_file('unifont.bdf')

def compute_text_bounds(text):
	assert unifont
	textbytes = bytes(str(text), 'UTF-8')
	textw, texth = c_size_t(0), c_size_t(0)
	res = bdf.framebuffer_get_text_bounds(textbytes, unifont, byref(textw), byref(texth))
	if res:
		raise ValueError('Invalid text')
	return textw.value, texth.value

def render_text(text, offset):
	cbuf = create_string_buffer(FRAME_SIZE*sizeof(COLOR))
	textbytes = bytes(str(text), 'UTF-8')
	res = bdf.framebuffer_render_text(textbytes, unifont, cbuf, DISPLAY_WIDTH, DISPLAY_HEIGHT, offset)
	if res:
		raise ValueError('Invalid text')
	return np.ctypeslib.as_array(cast(cbuf, POINTER(c_uint8)), shape=(DISPLAY_WIDTH, DISPLAY_HEIGHT, 4))

printlock = threading.Lock()

def printframe(fb):
	w,h,_ = fb.shape
	printlock.acquire()
	print('\0337\033[H', end='')
	print('Rendering frame @{}'.format(time()))
	bdf.console_render_buffer(fb.ctypes.data_as(POINTER(c_uint8)), w, h)
	print('\033[0m\033[KCurrently rendering', current_entry.entrytype, 'from', current_entry.remote, ':', current_entry.text, '\0338', end='')
	printlock.release()

QueueEntry = namedtuple('QueueEntry', ['entrytype', 'remote', 'timestamp', 'text'])
defaultlines = [ QueueEntry('text', '127.0.0.1', 0, l[:-1].replace('\\x1B', '\x1B')) for l in open('default.lines').readlines() ]
random.shuffle(defaultlines)
defaulttexts = cycle(defaultlines)
current_entry = next(defaulttexts)
conns = {}
textqueue = []

def log(*args):
	printlock.acquire()
	print(strftime('\x1B[93m[%m-%d %H:%M:%S]\x1B[0m'), ' '.join(str(arg) for arg in args), '\x1B[0m')
	printlock.release()

class MateLightUDPServer(UDPServer):
	"""
	The server class for the SocketServer interface.
	"""
	def __init__(self, *args, **kwargs):
		"""
		Setup the deque for the frame
		"""
		super(MateLightUDPServer, self).__init__(*args, **kwargs)
		self.frame_deque = deque(maxlen=30)
		
	def get_next_frame(self):
		try:
			return self.frame_deque.popleft()
		except IndexError:
			return None

class MateLightTCPServer(TCPServer):
	def __init__(self, *args, **kwargs):
		super(MateLightTCPServer, self).__init__(*args, **kwargs)
		# A deque for the texts, contains a tuple of the form (text, width, height)
		self.text_deque = deque(maxlen=32)
		self.current_text = None

	def get_next_frame(self):
		if self.current_text is None or self.i > self.current_text[1][0]:
			self.current_text = None
			try:
				self.current_text = self.text_deque.popleft()
				self.i = -DISPLAY_WIDTH
			except IndexError:
				return None

		frame = render_text(self.current_text[0], self.i)
		self.i += 1
		return frame

class MateLightUDPHandler(BaseRequestHandler):
	"""
	Handles one UDP connection to the matelight
	"""
	def handle(self):
		try:
			# Housekeeping - FIXME: This is *so* the wrong place for this.
			for k,v in conns.items():
				if time() - v.timestamp > UDP_TIMEOUT:
					del conns[k]

			global current_entry, conns
			data = self.request[0].strip()
			if len(data) != FRAME_SIZE*3+4:
				#raise ValueError('Invalid frame size: Expected {}, got {}'.format(FRAME_SIZE+4, len(data)))
				return
			frame = data[:-4]
			#crc1, = struct.unpack('!I', data[-4:])
			#crc2, = zlib.crc32(frame, 0),
			#if crc1 != crc2:
			#	raise ValueError('Invalid frame CRC checksum: Expected {}, got {}'.format(crc2, crc1))
			#socket.sendto(b'ACK', self.client_address)
			a = np.array(list(frame), dtype=np.uint8)
			timestamp = time()
			addr = self.client_address[0]
			conn = QueueEntry('udp', addr, timestamp, '')
			if addr not in conns:
				log('\x1B[91mNew UDP connection from\x1B[0m', addr)
				current_entry = conn
			conns[addr] = current_entry
			if current_entry.entrytype == 'udp' and current_entry.remote == addr:
				current_entry = conn
				frame = a.reshape((DISPLAY_WIDTH, DISPLAY_HEIGHT, 3))
				self.server.frame_deque.append(frame)
				#sendframe(frame)
				printframe(np.pad(frame, ((0,0),(0,0),(0,1)), 'constant', constant_values=(0,0)))
		except Exception as e:
			log('Error receiving UDP frame:', e)

class MateLightTCPTextHandler(BaseRequestHandler):
	def handle(self):
		global current_entry, conns
		data = str(self.request.recv(1024).strip(), 'UTF-8')
		addr = self.client_address[0]
		timestamp = time()
		if len(data) > 140:
			self.request.sendall('TOO MUCH INFORMATION!\n')
			return
		log('\x1B[95mText from\x1B[0m {}: {}\x1B[0m'.format(addr, data))
		#textqueue.append(QueueEntry('text', addr, timestamp, data))
		self.server.text_deque.append((data, compute_text_bounds(data)))
		self.request.sendall(b'KTHXBYE!\n')

TCPServer.allow_reuse_address = True
tserver = MateLightTCPServer(('', 1337), MateLightTCPTextHandler)
t = threading.Thread(target=tserver.serve_forever)
t.daemon = True
t.start()

UDPServer.allow_reuse_address = True
userver = MateLightUDPServer(('', 1337), MateLightUDPHandler)
t = threading.Thread(target=userver.serve_forever)
t.daemon = True
t.start()

def udp_sub_loop():
	"""
	If we can get at least one frame from get_next_frame() we will wait
	for 1000 frame cycles till we return. If we do not get a valid frame
	in the first loop, we return immediatly.
	"""
	invalid_frames = 300
	while True:
		next_frame = userver.get_next_frame()
		if next_frame is not None:
			invalid_frames = 0
			sendframe(next_frame)
			sleep(0.01)
		else:
			invalid_frames += 1
			sleep(0.01)
		if invalid_frames > 300:
			break

def tcp_sub_loop():
	while True:
		next_frame = tserver.get_next_frame()
		if next_frame is not None:
			sendframe(next_frame)
		else:
			break

if __name__ == '__main__':
	while True:
		udp_sub_loop()
		tcp_sub_loop()
		

def bla():
	print('\033[2J'+'\n'*9)
	while True:
		if current_entry.entrytype == 'text':
			if scroll(current_entry.text):
				if current_entry in textqueue:
					textqueue.remove(current_entry)
				if textqueue:
					current_entry = textqueue[0]
				else:
					if conns:
						current_entry = random.choice(list(conns.values()))
					else:
						current_entry = next(defaulttexts)
			if current_entry.entrytype != 'udp' and textqueue:
				current_entry = textqueue[0]
				log('\x1B[92mScrolling\x1B[0m', current_entry.text)
		if current_entry.entrytype == 'udp':
			if time() - current_entry.timestamp > UDP_TIMEOUT:
				current_entry = next(defaulttexts)
			else:
				sleep(0.1)

