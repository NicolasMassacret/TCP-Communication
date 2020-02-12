'''
	Simple socket server using threads
'''

import socket
import sys

HOST = ''	# Symbolic name, meaning all available interfaces
PORT = 8888	# Arbitrary non-privileged port

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print 'Socket created'

#Bind socket to local host and port
try:
	s.bind((HOST, PORT))
except socket.error as msg:
	print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
	sys.exit()
	
print 'Socket bind complete'

#Start listening on socket
s.listen(10)
print 'Socket now listening'

#now keep talking with the client
while 1:
    #wait to accept a connection - blocking call
	conn, addr = s.accept()
	print 'Connected with ' + addr[0] + ':' + str(addr[1])
	try:
                print >>sys.stderr, 'client connected:', addr
                while True:
                        data = conn.recv(16)
                        print >>sys.stderr, 'received "%s"' % data
                        if data:
                                conn.sendall("LabView here!")
                        else:
                                break
        finally:
                conn.close()
s.close()
