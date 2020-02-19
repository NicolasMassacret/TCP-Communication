'''
	Simple socket server using threads
'''

import socket
import sys

HOST = 'localhost'	# Symbolic name, meaning all available interfaces
# HOST = ''	# Symbolic name, meaning all available interfaces
PORT = 8888	# Arbitrary non-privileged port

vars =
{
        'rotpos' : 4711,
        'transpos' : 12345,
        'busy' : 1
}

settings =
{
        'rotpos' : 0,
        'transpos' : 0,
        'homerot' : 0
}


def answer(msg):
        msg = msg.strip("\r\n ")
        print >>sys.stderr, 'received "%s"' % msg
        if(msg == "MIDAS"):
                conn.sendall("LabView(fake)")
        else:
                (cmd,arg) = msg.split('_',2)
                print cmd, arg
                if(cmd == "list" and arg == "vars"):
                        conn.sendall(varlist)
                elif(cmd == "read"):
                        val = 4711
                        conn.sendall(str(val))

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
                        data = conn.recv(80)

                        if data:
                                answer(data)
                        else:
                                break
        finally:
                conn.close()
s.close()
