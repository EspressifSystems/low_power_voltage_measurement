
import datetime
import time
import os
import binascii
import string
import httplib
import urllib2
import json

DATETIME_FORMAT = '%Y%m%d-%H-%M-%S'
now = datetime.datetime.now()
dir_datetime = now.strftime(DATETIME_FORMAT)


def get_test_data(devCnt=10,key='4f5bbb332f838b33cd6211a11bde561f06858c78',start='',end='',offset=0,data_stream="supply_voltage",data_num_in_set=20):
    devices_count =devCnt
    devKey = key
    start_time = start
    end_time   = end
    offset = offset
    data_stream = data_stream

    host = 'iot.espressif.cn'
    headers = {'Host': host, 'User-Agent': 'Mozilla', 'Accept': '*/*', 'Authorization': 'token %s' % devKey}
    params=None
    if start_time=='':
        path = '/v1/datastreams/%s/datapoints/?row_count=%d' % (data_stream, devices_count )
    elif offset == 0:
        path = '/v1/datastreams/%s/datapoints/?row_count=%d&start=%s&end=%s' % (data_stream, devices_count, start_time, end_time)
    else:
        path = '/v1/datastreams/%s/datapoints/?row_count=%d&start=%s&end=%s&offset=%d' % (data_stream, devices_count, start_time, end_time, offset)
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
    time_cal_list = [ str(datetime.datetime.strptime(time_list[i],'%Y-%m-%d %H:%M:%S') - datetime.timedelta(minutes=((data_num_in_set-1)-idx_list[i])))  for i in range(len(val_data_list))] 
    voltage_list = [ i*1.0/1024 for i in val_data_list]
    time_cal_list.reverse()
    voltage_list.reverse()
    print time_cal_list
    print voltage_list
    



if __name__ == '__main__':
    #devCnt : data num (<1000)
    #key: sensor devkey
    #start: data start time 
    #end : data end time
    #offset:  data page offset , (if data num exceed 1000: offset++)
    #data_stream : data_stream name on the server     
    #key = '4f5bbb332f838b33cd6211a11bde561f06858c78' #device
    #key = '976f39a53c354de37e58f527c232c92b8f67ad12' #owner
    #key = 'e50d9d278b6953ff761db08ec37d3de09cb81577' #madcowmoomoo
    key = open('../bin/key.bin','r').readline().replace('\xFF','')
    print key
    get_test_data(devCnt=100,
                  key=key,
                  start='2015-03-15 00:00:00',
                  end=dir_datetime,
                  offset=1,
                  data_num_in_set = 20,  #data number uploaded one time
                  data_stream="supply-voltage")
    
    
