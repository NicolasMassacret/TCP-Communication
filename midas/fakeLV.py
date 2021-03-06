#!/usr/bin/env python
'''
	Socket server mimicking Nicolas Demers' LabView server
'''

import socket
import sys
import argparse

# HOST = ''	# Symbolic name, meaning all available interfaces

## Fake LabView read-only variables
vars = {
        'RotPos' : ['I64', 4711],
        'RotPos2' : ['I32', 4712],
        'TransPos' : ['Double Float',12345],
        'Busy' : ['Boolean', 1],
        'MyString' : ['String', 'StringVal'],
        'MyChar' : ['U8', 17]
}

## Fake LabView read/write settings
settings = {
        'RotPos' : ['I64', 0],
        'RotPos2' : ['I32', 0],
        'TransPos' : ['Double Float', 0],
        'HomeRot' : ['Boolean', 0]
}


def answer(msg):
        """
        Respond to messages from Midas frontend for LabView.

        Supported commands:

        list_vars to receive a list of available variables
        <varname>:? to query value of variable <varname>
        <varname>:<value> to change value of variable <varname>
        """
        msg = msg.strip("\r\n ")
        print >>sys.stderr, 'received "%s"' % msg
        if(msg == "midas"):
                conn.sendall("labview(fake)\r\n")
        elif(msg == "list_vars"):
                varlist = ""
                for key in vars:
                        varlist = varlist + key + ":" + vars[key][0] + ":V;"
                        print(varlist)
                for key in settings:
                        varlist = varlist + key + ":" + settings[key][0] + ":S;"
                        print(varlist)
                conn.sendall(varlist + "\r\n")
        else:
                (cmd,arg) = msg.split(':',2)
                print cmd, arg
                if(arg == "?"):
                        if(cmd in vars):
                                conn.sendall(cmd + ":" + str(vars[cmd][1]) + "\r\n")
                        elif(cmd in settings):
                                conn.sendall(cmd + ":" + str(settings[cmd][1]) + "\r\n")
                        else:
                                print "Unknown variable:", cmd
                elif(cmd in settings):
                        settings[cmd][1] = arg
                        print "Changed", cmd, "to", arg
                else:
                        print "Unknown command:", cmd


argparser = argparse.ArgumentParser()
argparser.add_argument("-H","--host",help="Host for the server socket to be, default localhost",type=str,default="localhost")
argparser.add_argument("-p","--port",help="Port for the server socket, default 8888",type=int,default=8888)
args = argparser.parse_args()

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print 'Socket created'

#Bind socket to local host and port
try:
	s.bind((args.host, args.port))
except socket.error as msg:
	print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
	sys.exit()

print 'Socket bind complete'

#Start listening on socket
try:
        s.listen(10)
        print 'Socket now listening on', args.host, ":", args.port

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
except KeyboardInterrupt:
        s.close()
        print('Bye!')
        pass
s.close()
