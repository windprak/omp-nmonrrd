# omp-nmonrrd
nmon2rrd for linux on mainframe


First install rrdtool:

sudo yum install rrdtool


Then we have to compile nmon for mainframe:

wget http://ncu.dl.sourceforge.net/project/nmon/lmon16f.c

wget http://ncu.dl.sourceforge.net/project/nmon/makefile

nmon_mainframe_rhel7: $(FILE)
	cc -o nmon_mainframe_rhel7 $(FILE) $(CFLAGS) $(LDFLAGS) -D MAINFRAME -D KERNEL_2_6_18 -D RHEL7 
	
Create a nmon-file:  nmon -f  -s1 -c 500



Install httpd webserver:

sudo yum install httpd

open port: sudo iptables -I INPUT -p tcp --dport 80 -j ACCEPT


nmon2rrd:

Download nmon2rrdomp.c

gcc -Wno-implicit-int -o nmon2rrd nmon2rrdomp.c

./nmon2rrd -f filename.nmon -d /var/www/html -x
