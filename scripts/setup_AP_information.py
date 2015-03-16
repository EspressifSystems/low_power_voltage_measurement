import httplib, urllib
import json
import time


def config_device(ssid,password,token,softap_ip):
    httpClient = None
    config_json = \
    {
        "Request":{
        "Station":{
        "Connect_Station":{
        "ssid":ssid,
        "password":password,
          "token":token
    }}}}
    
    
    #params = urllib.urlencode(config_json)  
    #print "test: params: ",params
    params = json.dumps(config_json)
    
    if True:
        try:
            headers = {"Content-type": "application/json", "Accept": "text/plain","Connection": "keep-alive","token":"1234567890123456789012345678901234567890"}
            httpClient = httplib.HTTPConnection(softap_ip)
            httpClient.request("POST", "/config?command=wifi", params, headers)
            #response = httpClient.getresponse()

        except Exception, e:
            print e
        finally:
            if httpClient:
                httpClient.close()



if __name__ =="__main__":
    #params:
    #ssid: target router ssid
    #password: router password
    #toke : a random value used to register to the server , 40bytes, just use the default value in this case
    #softap_ip: ip of the device softap interface
    
    #steps:
    #1.connect to the softap of device
    #2.set the router params: ssid and password
    #3.run&done
    
    config_device(ssid="WIFI_SSID",
                  password="YOU_WIFI_PASSWORD",
                  token="1234567890123456789012345678901234567890",
                  softap_ip="192.168.4.1")
    
