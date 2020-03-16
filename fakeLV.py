'''
	Simple socket server using threads
'''

import socket
import sys

HOST = 'localhost'	# Symbolic name, meaning all available interfaces
# HOST = ''	# Symbolic name, meaning all available interfaces
PORT = 8888	# Arbitrary non-privileged port

vars = {
        'RDRotPos' : 4711,
        'RDTransPos' : 12345,
        'RBBusy' : 1
}

settings = {
        'WDRotPos' : 0,
        'WDTransPos' : 0,
        'WBHomeRot' : 0
}


def answer(msg):
        msg = msg.strip("\r\n ")
        print >>sys.stderr, 'received "%s"' % msg
        if(msg == "midas"):
                conn.sendall("labview(fake)\r\n")
        else:
                (cmd,arg) = msg.split('_',2)
                print cmd, arg
                if(cmd == "list" and arg == "vars"):
                        varlist = ""
                        for key in vars:
                                varlist = varlist + "_" + key
                        for key in settings:
                                varlist = varlist + "_" + key
                        conn.sendall(varlist + "\r\n")
                elif(arg == "?"):
                        if(cmd in vars):
                                conn.sendall(cmd + "_" + str(vars[cmd]) + "\r\n")
                        elif(cmd in settings):
                                conn.sendall(cmd + "_" + str(settings[cmd]) + "\r\n")
                        else:
                                print "Unknown variable:", cmd
                elif(cmd in settings):
                        settings[cmd] = arg
                        print "Changed", cmd, "to", arg
                else:
                        print "Unknown command:", cmd

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
