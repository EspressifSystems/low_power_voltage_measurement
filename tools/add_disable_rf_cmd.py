#!/usr/bin/python
#
# File  : add_disable_rf_cmd.py
# This file is part of Espressif's generate bin script and based on the works of 
#    Fredrik Ahlberg. The original version can be found at: 
#    ESP8266 ROM Bootloader Utility <https://github.com/themadinventor/esptool>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT 
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301 USA.

import struct
import os,sys

class ESPROM:

    # These are the currently known commands supported by the ROM
    ESP_FLASH_BEGIN = 0x02
    ESP_FLASH_DATA  = 0x03
    ESP_FLASH_END   = 0x04
    ESP_MEM_BEGIN   = 0x05
    ESP_MEM_END     = 0x06
    ESP_MEM_DATA    = 0x07
    ESP_SYNC        = 0x08
    ESP_WRITE_REG   = 0x09
    ESP_READ_REG    = 0x0a

    # Maximum block sized for RAM and Flash writes, respectively.
    ESP_RAM_BLOCK   = 0x1800
    ESP_FLASH_BLOCK = 0x400

    # Default baudrate used by the ROM. Don't know if it is possible to change.
    ESP_ROM_BAUD    = 115200

    # First byte of the application image
    ESP_IMAGE_MAGIC = 0xe9
    BIN_MAGIC_IROM   = 0xEA

    # Initial state for the checksum routine
    ESP_CHECKSUM_MAGIC = 0xef
    
    ESP_MAC = ""
    
    #download state
    ESP_DL_OK = 0x0
    ESP_DL_IDLE = 0x1
    ESP_DL_CONNECT_ERROR = 0x2
    ESP_DL_SYNC = 0x3
    ESP_DL_SYNC_ERROR = 0x4
    ESP_DL_ERASE = 0x5
    ESP_DL_ERASE_ERROR = 0x6
    ESP_DL_DOWNLOADING = 0x7
    ESP_DL_DOWNLOAD_ERROR = 0x8
    ESP_DL_FAIL = 0x9
    ESP_DL_FINISH = 0xA
    ESP_DL_STOP = 0xB
    

    def __init__(self, frame, port = 6, baudrate = 115200):
        self._COM = port
        self.ESP_ROM_BAUD = baudrate
        self.isOpen =  False
        self.stopFlg = False
        self.state = self.ESP_DL_IDLE
        self.process_num = 0
        self.MAC2 = 0
        self.MAC3 = 0
        self.MAC4 = 0
        self.MAC5 = 0
        self.frame = frame
        
        print "_COM: ",self._COM
        print "ESP_ROM_BAUD : ",self.ESP_ROM_BAUD
    @staticmethod
    def checksum(data, state = ESP_CHECKSUM_MAGIC):
        for b in data:
            state ^= ord(b)
        return state        

class ESPFirmwareImage:
    
    def __init__(self, filename = None):
        self.segments = []
        self.entrypoint = 0
	self.byte2 = 0
	self.byte3 = 0
	self.tail = 0

        if filename is not None:
	    print "test file in fwi: ",filename
            f = file(filename, 'rb')
            (magic, segments, self.byte2, self.byte3, self.entrypoint) = struct.unpack('<BBBBI', f.read(8))
            
            # some sanity check
            #if (magic != ESPROM.ESP_IMAGE_MAGIC and magic != ESPROM.BIN_MAGIC_IROM) or segments > 16:
	    if magic != ESPROM.ESP_IMAGE_MAGIC or segments > 16:
                raise Exception('Invalid firmware image;magic:%d'%magic)
		#pass
        
            for i in xrange(segments):
                (offset, size) = struct.unpack('<II', f.read(8))
                if offset > 0x40200000 or offset < 0x3ffe0000 or size > 65536:
                    raise Exception('Suspicious segment %x,%d' % (offset, size))
		    #pass
		
                self.segments.append((offset, size, f.read(size)))

            # Skip the padding. The checksum is stored in the last byte so that the
            # file is a multiple of 16 bytes.
            align = 15-(f.tell() % 16)
            f.seek(align, 1)

            self.checksum = ord(f.read(1))

    def add_segment(self, addr, data):
        #self.segments.append((addr, len(data), data))
	self.segments = [ (addr,len(data),data),]+self.segments

    def save(self, filename):
        f = file(filename, 'wb')
        f.write(struct.pack('<BBBBI', ESPROM.ESP_IMAGE_MAGIC, len(self.segments), self.byte2, self.byte3, self.entrypoint))

        checksum = ESPROM.ESP_CHECKSUM_MAGIC
        for (offset, size, data) in self.segments:
            f.write(struct.pack('<II', offset, size))
            f.write(data)
            checksum = ESPROM.checksum(data, checksum)

        align = 15-(f.tell() % 16)
        f.seek(align, 1)
        f.write(struct.pack('B', checksum))
	f.close()


 
def add_instruction(bin_file='eagle.app.v6.flash.bin',segment_list=[]):
    fw = ESPFirmwareImage(bin_file)
    #for (addr,data) in segment_list:
    len_list = len(segment_list)
    for i in range(len_list):
        (addr,data)=segment_list[len_list-1-i]
        
        fw.add_segment(addr,data)
    #print "test segs:"
    #for (addr,length,data) in fw.segments:
	    #print "addr:0x%08x ; len :0x%08x  %d kB"%(addr,length,length/1024)
    new_name = bin_file[:-4]+'_disable_rf.bin'
    #print "file name : ",bin_file
    print "generated : "+new_name
    fw.save(new_name)
    
    size_f1=os.path.getsize(bin_file)
    #print "size f1: ",size_f1
    size_f2=os.path.getsize(new_name)
    #print "size f2: ",size_f2
    if(size_f1>size_f2):
	f1=open(bin_file,'rb')
	f2=open(new_name,'ab')
	data_append = f1.read(size_f1)
	f2.write(data_append[size_f2:] )
	f1.close()
	f2.close()
    
    
    

def add_disable_rf():
    bin_file_name = ''
    try:
        bin_file_name = str(sys.argv[1])
        print "file name : ",bin_file_name
    except:
        print "bin path error"

    if not "new.bin" in bin_file_name:
	try:
	    add_instruction(bin_file=bin_file_name,
		    segment_list=[(0x60000710,struct.pack('<i',0x00000000)),#disable rf 
		                  ])
	except:
	    print 'add command error...' 
    else:
	print "this version of 'boot_v1.2.bin' do not support disable rf during booting for now..."
	
    
    
if __name__=="__main__":
    add_disable_rf()

