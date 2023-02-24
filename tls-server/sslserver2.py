#!/bin/env python
import ssl, socket

#context = ssl.SSLContext(ssl.PROTOCOL_TLSv1_2)
context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(certfile="certs/host.crt", keyfile="certs/host.key")

bindsocket = socket.socket()
bindsocket.bind(('0.0.0.0', 6000))
bindsocket.listen(5)

def deal_with_client(connstream):
    data = connstream.recv(1024)
    # empty data means the client is finished with us
    while data:
        print(data)
        data = connstream.recv(1024)

while True:
    newsocket, fromaddr = bindsocket.accept()
    connstream = context.wrap_socket(newsocket, server_side=True)
    try:
        deal_with_client(connstream)
    finally:
        connstream.shutdown(socket.SHUT_RDWR)
        connstream.close()