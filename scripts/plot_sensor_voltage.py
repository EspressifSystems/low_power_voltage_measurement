
import datetime
import time
import os
import binascii
import string
import httplib
import urllib2
import json
import matplotlib.pyplot as plt
import numpy as np

DATETIME_FORMAT = '%Y%m%d-%H-%M-%S'
now = datetime.datetime.now()
dir_datetime = now.strftime(DATETIME_FORMAT)


def get_test_data(key='26484cd14129a0c64ed1bf9608fec6cabb0fd0c9',start='',end='',offset=0, datastream='supply-voltage'):
    voltage_list = []
    time_cal_list = []
    end_tmp = end
    while True:
        [t,v] = get_test_data_sub(devCnt=1000,key=key,start=start,end=end_tmp,offset=offset,datastream=datastream)
        time_cal_list.extend(t)
        voltage_list.extend(v)
        if len(t)==0: break
        if not time_cal_list[-1]>datetime.datetime.strptime(start, '%Y-%m-%d %H:%M:%S'): break
        end_tmp = t[-1]

    #for i in range(len(voltage_list)-1):
    #    if voltage_list[i]<voltage_list[i+1]-0.01: 
    #        voltage_list[i]=voltage_list[i+1]
    
    plt.plot(time_cal_list,voltage_list,'r-')
    plt.plot(time_cal_list,voltage_list,'b.')
    plt.show()

def get_test_data_sub(devCnt=10,key='26484cd14129a0c64ed1bf9608fec6cabb0fd0c9',start='',end='',offset=0, datastream='supply-voltage'):
    devices_count =devCnt
    devKey = key
    start_time = start
    end_time   = end
    offset = offset

    host = 'iot.espressif.cn'
    headers = {'Host': host, 'User-Agent': 'Mozilla', 'Accept': '*/*', 'Authorization': 'token %s'%devKey}

    params = None#'{"debugs": ['+ ','.join(devices_serial) + ']}'
    if start_time=='':
        path = '/v1/datastreams/%s/datapoints/?row_count=%d'% (datastream, devices_count) 
    elif offset == 0:
        path = '/v1/datastreams/%s/datapoints/?row_count=%d&start=%s&end=%s' % (datastream, devices_count ,start_time,end_time)
    else:
        path = '/v1/datastreams/%s/datapoints/?row_count=%d&start=%s&end=%s&offset=%d' % (datastream, devices_count ,start_time,end_time,offset)
    conn = httplib.HTTPConnection(host, port=80)
    conn.request("GET", path, params, headers)
    response = conn.getresponse()
    data = response.read()
    conn.close()

    total_data_dict = json.loads(data)
    time_list=[]    
    idx_list = []
    val_data_list = []
    
    for dict_tmp in total_data_dict["datapoints"]:
        time_list.append( dict_tmp["updated"] )
        idx_list.append( dict_tmp['x'])
        val_data_list.append( dict_tmp['y'])
    time_cal_list = []
    for i in range(len(val_data_list)):
        time_cal_list.append( 
            datetime.datetime.strptime(time_list[i],'%Y-%m-%d %H:%M:%S') - 
            datetime.timedelta(minutes=((19-idx_list[i]))*60./60.)
        )
    #plt.plot(val_data_list)
    #plt.show()
    voltage_list = [ i/1024.0 for i in val_data_list]
    #print voltage_list
    return [time_cal_list, voltage_list]

    

if __name__ == '__main__':
    key = open('../bin/key.bin','r').readline().replace('\xFF','')
    print "Key is", key
    get_test_data(key=key, datastream='supply-voltage',
                  start='2015-04-20 13:30:00',
                  end=dir_datetime,offset=0)
    #devCnt : data num (<1000)
    #key: sensor devkey
    #start: data start time 
    #end : data end time
    #offset:  data page offset , (if data num exceed 1000: offset++)
