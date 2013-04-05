import os, re

total = []
written = -1
dis_written = -1
read = -1
dis_read = -1

time = -1

read_total = []
write_total = []

m1_total = []
m2_total = []

def extract_server():
    for i in range(3, 17):
        temp_read = []
        temp_write = []
        directory = "./{0}nodes/".format(i)
        for j in range(1, 3):
            total = []
            written = -1
            dis_written = -1
            read = -1
            dis_read = -1
            time = -1
            f = open(directory + "s{0}_result".format(j), "r")
            
            for line in f:
                m = re.search("bytes_read:\s+(.*)\n", line) 
                if m and m.group(1) != "inf":
                    d = float(m.group(1))
                    if read != -1:
                        dis_read = d - read
                        # print "read discrepency", dis_read
                    read = d

                m = re.search("bytes_written:\s+(.*)\n", line) 
                if m and m.group(1) != "inf":
                    d = float(m.group(1))
                    if written != -1:
                        dis_written = d - written
                        if dis_written < 0:
                            print "allert smaller than 0", dis_written
                        # print "write discrepency", dis_read
                    written = d

                m = re.search("time:\s+(.*)\n", line) 
                if m and m.group(1) != "inf":
                    d = float(m.group(1))
                    if time != -1:
                        dis = d - time
                        if dis > 0:
                            temp_read.append(dis_read/dis) 
                            temp_write.append(dis_written/dis)
                    time = d

                m = re.search("MemFree:(.*)\n", line) 
                if m:
                    if j == 1:
                        m1_total.append(float(m.group(1)))
                    elif j == 2:
                        m2_total.append(float(m.group(1)))
                    # print line, m.group(1)
        read_total.append(sum(temp_read)/len(temp_read))
        write_total.append(sum(temp_write)/len(temp_write))


    # print sum(total)/len(total)


def extract_latency():
    for i in range(3, 17):
        directory = "./{0}nodes/".format(i)
        for root, dirs, files in os.walk(directory):
            for file in files:
                if file != "command.sh" and file != "extract_info.py":
                    f = open(directory + file, "r")
                    for line in f:
                        m = re.search("avg:\s(.*),", line) 
                        if m and m.group(1) != "inf":
                            total.append(float(m.group(1)))
        print sum(total)/len(total)


extract_server()
print "read===="
for i in read_total:
    print i
print "write ===="
for i in write_total:
    print i
print "mem1 free ==="
for i in m1_total:
    print i
print "mem2 free ==="
for i in m2_total:
    print i


