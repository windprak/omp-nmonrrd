/* This is a sample code filter program to convert a nmon performance data output file to rrdtool input format */
/* This version 16 has drastic changes which extracts column header names fromthe nmon file which makes it less sensitive to new columns */
/* It has also been beautified  with the AIX cb -s command to fix indentation mistakes */
/* Change comments (peoples names and company names) removed - this is sample code */

#define VERSION "20a"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
/*Linux-version*/ 
#include <string.h>
/*Variable for Linux Mode*/
int   linuxone = 0;
int	debug = 0; /* set to a possitive number to switch on output */
int	interval, snapshots;
int	grwidth = 800;
int	grheight = 300;
int	longest = 0;
/* +++++++++++++++++++++++++++++++++++++ UTC Stuff here */
long	tarray[10240];

char	dirname[1024] = "./";
char	filename[1024];

char	*months[12] = {
	"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
	"JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

int backward = 0; /* if 1 then use YYYY/MM/DD date format for the Americans */

int	no_of_nmon_files = 1; /* used to make the rrdtool database larger to load multiple nmon files */
int	generic_graph_output = 0; /* used to make the rrd_graph output file contain easily changeable to cover all the data in the rrd daabase */

#define TOP_PROCESSES 20 /* should be a multiple of 4 */

long	utc(int sec, int min, int hour, int day, int mon, int year)
{
	struct tm *timp;
	time_t timt;

	timt = time(0);
	timp = localtime(&timt);
	timp->tm_hour = hour;
	timp->tm_min = min;
	timp->tm_sec = sec;
	timp->tm_mday = day;
	timp->tm_mon = mon,
	    timp->tm_year = year;

	timt = mktime(timp);
	if (debug)
		printf("%d:%02d.%02d %02d/%02d/%04d = %ld\n", timp->tm_hour, timp->tm_min, timp->tm_sec, timp->tm_mday,
		    timp->tm_mon + 1, timp->
		    tm_year + 1900, timt);
	return (long)timt;
}


int search_for_tstring(char *s)
{
	long	len = strlen(s);
	int	i;

	for (i = 0; i < len; i++) {
		if ( s[i] == ',' && 
		    s[i+1] == 'T' && 
		    isdigit(s[i+2]) && 
		    isdigit(s[i+3]) && 
		    isdigit(s[i+4]) && 
		    isdigit(s[i+5]) && 
		    s[i+6] == ',' )
			return i + 1;

	}
	return 0;
}


/* Top Disk Data */
#define NUMOFNAMES 1024
#define NUMOFSHOTS 300
#define NUMOFITEMS (NUMOFNAMES * NUMOFSHOTS)

struct topdisk {
	float	min;
	float	max;
	float	percent;
	int	hits;
	char	*name;
} topdisk[NUMOFNAMES];

int	topdisks = NUMOFNAMES;
int	diskrows = 0;

/* Top Disk Code */
void start_disks()
{
	int	i;
	for (i = 0; i < topdisks; i++) {
		topdisk[i].min = 100.0;
		topdisk[i].max = -1.0;
		topdisk[i].percent = 0.0;
		topdisk[i].hits = 0;
	}
}


void save_diskname(int disk, char *name)
{
	topdisk[disk].name = malloc(strlen(name) + 1);
	strcpy(topdisk[disk].name, name);

}


void save_disk(int disk, int percent)
{
	if (percent > topdisk[disk].max)
		topdisk[disk].max = percent;
	if (percent < topdisk[disk].min)
		topdisk[disk].min = percent;
	topdisk[disk].percent += percent;
	topdisk[disk].hits++;
}


int compare_disk( const void *a, const void *b)
{
	return (int)((((struct topdisk *)b)->percent * 1000)  - (((struct topdisk *)a)->percent * 1000) );
}


void print_disk()
{
	int	i;
	qsort((void *) & topdisk[0], topdisks, sizeof(struct topdisk ), &compare_disk);
	for (i = 0; i < topdisks; i++) {
		if (topdisk[i].max == -1.0)
			return;
		printf("disk %d min=%5.2f max=%5.2f totalpercent=%5.2f hits=%d\n avg=%5.2f wavg=%5.2f",
		    i,
		    topdisk[i].min,
		    topdisk[i].max,
		    topdisk[i].percent,
		    topdisk[i].hits,
		    topdisk[i].percent / diskrows,
		    topdisk[i].percent / topdisk[i].hits
		    );
	}
}


/* Top Processes Data */
struct top {
	float	cpu;
	char	*name;
	int	tnum;
} tops[NUMOFITEMS];

int	topnum = NUMOFITEMS;

struct topname {
	float	cpu;
	float	tmp;
	char	*name;
} topname[NUMOFNAMES];

int	topnames = NUMOFNAMES;

/* Top Processes Code */
int compare_top( const void *a, const void *b)
{
	return (int)((((struct topname *)b)->cpu * 1000)  - (((struct topname *)a)->cpu * 1000) );
}


void	start_top(int num)
{
	int	i;
	for (i = 0; i < topnum; i++)
		tops[i].tnum = -1;
	for (i = 0; i < topnames; i++) {
		topname[i].cpu = -1;
		topname[i].name = 0;
	}
}

void	straddch(char *s, char ch)
{
	int	len = strlen(s);
	s[len] = ch;
	s[len+1] = 0;
}


void replace1(char *orig, char *old, char *new)
{
	int	j;
	int	len;
	int	oldlen;
	char	*s;
	s = malloc(longest);
	oldlen = strlen(old);
	strcpy(s, orig);
	orig[0] = 0;
	len = strlen(s);
	for (j = 0; j < len; j++) {
		if ( !strncmp(&s[j], old, oldlen)) {
			strcat(orig, new);
			j = j + oldlen - 1;
		} else
			straddch(orig, s[j]);
	}
	if (debug)
		printf("replaced %s with %s\n", s, orig);
	free(s);
}






void	save_top(int tnum, float busy, char *name)
{
	int	i;
	if (busy < 0.0001)
		return;

	/* clean up the name */
	if (!strncmp("gil = TCP/IP", name, 12))
		name[3] = 0;
	if (!strncmp("defunct", name, 7))
		name[7] = 0;
	replace1(name, "+", "");
	replace1(name, ".", "");
	replace1(name, "-", "");
	replace1(name, " ", "");
	for (i = 0; i < topnum; i++) {
		if ( tops[i].tnum < 0) { /* no match */
			tops[i].name = malloc(strlen(name) + 1);
			strcpy(tops[i].name, name);
			tops[i].cpu = busy;
			tops[i].tnum = tarray[tnum];
			if (debug)
				printf("topnew %d %7.2f cmd=%s.\n", tnum, busy, name);
			break;
		}
		if ( tops[i].tnum == tarray[tnum] && !strcmp(name, tops[i].name)) {
			tops[i].cpu += busy;
			if (debug)
				printf("topadd %d %7.2f cmd=%s. now=%7.2f\n", tnum, busy, name, tops[i].cpu);
			break;
		}
	}
	for (i = 0; i < topnames; i++) {
		if ( topname[i].cpu < 0) {
			topname[i].name = malloc(strlen(name) + 1);
			strcpy(topname[i].name, name);
			topname[i].cpu = busy;
			break;
		}
		if ( !strcmp(name, topname[i].name)) {
			topname[i].cpu += busy;
			break;
		}
	}
	if (debug)
		printf("topname i=%d  %7.2f cmd=<%s> now=%7.2f\n", i, busy, name, topname[i].cpu);
}


void	end_top(void)
{
	int	i;
	int	j;
	int	current;
	FILE * tfp;

	qsort((void *) & topname[0], topnames, sizeof(struct topname ), &compare_top);
	if (debug)
		for (i = 0; i < topnum; i++) {
			if ( tops[i].tnum < 0)
				break;
			printf("top:%d %7.2f cmd=%s.\n",
			    tops[i].tnum, tops[i].cpu, tops[i].name);
		}
	current = tops[0].tnum;
	for (j = 0; j < TOP_PROCESSES; j++) {
		topname[j].tmp = 0.0;
	}
	sprintf(filename, "%s/%s", dirname, "rrd_top");
	tfp = fopen(filename, "w");
	if (tfp == NULL) {
		perror("failed to open file");
		printf("file: \"%s\"\n", filename);
		exit(72);
	}
	for (i = 0; i < topnum; i++) {
		if ( tops[i].tnum < 0)
			break;
		if (tops[i].tnum != current) { /* then print this entry */
			fprintf(tfp, "update top.rrd %d", current);
			for (j = 0; j < TOP_PROCESSES; j++) {
				fprintf(tfp, ":%.2f", topname[j].tmp);
				topname[j].tmp = 0.0;
			}
			fprintf(tfp, "\n");
			current = tops[i].tnum;
		}
		/* save this one */
		for (j = 0; j < TOP_PROCESSES; j++) {
			if (!strcmp(tops[i].name, topname[j].name)) {
				topname[j].tmp = tops[i].cpu;
				break;
			}
		}
	}
	fclose(tfp);
}


/* General use buffer */
#define STRLEN 8196
char	string[STRLEN];

/* UTC */
int	utc_start;
int	utc_end;

/* Static Variable lists for nmon sections */
char	*a_cpu[] = {
	"User", "Sys", "Wait", "Idle"};
char         *l_cpu[] = {"User","Sys","Wait","Idle","Steal"};
int	l_cpu_size = sizeof(l_cpu)
 / sizeof(char *);
char         *test[]={"User","Sys","Wait","Idle","Steal"};
int	a_cpu_size = sizeof(a_cpu)
 / sizeof(char *);

int	mem_size = 0;

/* Linux version */
char *a_page[] = {"nr_dirty","nr_writeback","nr_unstable","nr_page_table_pages","nr_mapped","nr_slab","pgpgin","pgpgout","pswpin","pswpout","pgfree","pgactivate","pgdeactivate","pgfault","pgmajfault","pginodesteal","slabs_scanned","kswapd_steal","kswapd_inodesteal","pageoutrun","allocstall","pgrotated","pgalloc_high","pgalloc_normal","pgalloc_dma","pgrefill_high","pgrefill_normal","pgrefill_dma","pgsteal_high","pgsteal_normal","pgsteal_dma","pgscan_kswapd_high","pgscan_kswapd_normal","pgscan_kswapd_dma","pgscan_direct_high","pgscan_direct_normal","pgscan_direct_dma"};

int a_page_size = sizeof(a_page)/sizeof(char *);

char *a_mem15[] = { "memtotal","hightotal","lowtotal","swaptotal","memfree","highfree","lowfree","swapfree","memshared","cached","active","bigfree","buffers","swapcached","inactive" };
int a_mem_size15 = sizeof(a_mem15)/sizeof(char *);

int	memuse_size = 0;
int	page_size = 0;

char	*a_file[] = {
	"iget", "namei", "dirblk", "readch", "writech", "ttyrawch", "ttycanch", "ttyoutch" };


int	a_file_size = sizeof(a_file)
 / sizeof(char *);

char	*a_aio[] = {
	"AIO_Servers", "AIO_Running", "AIO_cpu"};


int	a_aio_size = sizeof(a_aio)
 / sizeof(char *);

char *a_memnew[] = { "Process","FScache","System","Free","Pinned","User" };
int	memnew_size = 0;
int	ame_found = 0;

/* NFS */
char	*a_nfssvrv2[] = {
	"null", "getattr", "setattr", "root", "lookup", "readlink", "read", "wrcache", "write", "create", "remove", "rename",
	     	     "link", "symlink", "mkdir", "rmdir", "readdir", "fsstat" };
int	a_nfssvrv2_size = sizeof(a_nfssvrv2) / sizeof(char *);

char	*a_nfssvrv3[] = {
	"null", "getattr", "setattr", "lookup", "access", "readlink", "read", "write", "create", "mkdir", "symlink", "mknod",
	     	     "remove", "rmdir", "rename", "link", "readdir", "readdirp", "fsstat", "fsinfo", "pathconf", "commit" };
int	a_nfssvrv3_size = sizeof(a_nfssvrv3) / sizeof(char *);

char	*a_nfssvrv4[] = {
	"access", "clientid_confirm", "close", "commit", "compound", "create", "delegpurge", "delegreturn", "getattr", "getfh", 
		"link", "lock", "lockt", "locku", "lookup", "lookupp", "null", "nverify", "open", "openattr", "open_confirm", 
		"open_downgrade", "operations", "putfh", "putpubfh", "putrootfh", "read", "readdir", "readlink", "release_lock", 
		"remove", "rename", "renew", "restorefh", "savefh", "secinfo", "setattr", "set_clientid", "verify", "write" };
int	a_nfssvrv4_size = sizeof(a_nfssvrv4) / sizeof(char *);

char	*a_nfscliv2[] = {
	"null", "getattr", "setattr", "root", "lookup", "readlink", "read", "wrcache", "write", "create", "remove", "rename",
	     	     "link", "symlink", "mkdir", "rmdir", "readdir", "fsstat" };
int	a_nfscliv2_size = sizeof(a_nfscliv2) / sizeof(char *);

char	*a_nfscliv3[] = {
	"null", "getattr", "setattr", "lookup", "access", "readlink", "read", "write", "create", "mkdir", "symlink", "mknod",
	     	     "remove", "rmdir", "rename", "link", "readdir", "readdirp", "fsstat", "fsinfo", "pathconf", "commit" };
int	a_nfscliv3_size = sizeof(a_nfscliv3) / sizeof(char *);

char	*a_nfscliv4[] = {
	"access", "acl_read", "acl_stat_l", "acl_write", "client_confirm", "close", "commit", "create", "delegreturn", "finfo", 
		"getattr", "link", "lock", "lock_test", "lookup", "mkdir", "mknod", "null", "open", "open_confirm", "open_downgrade", 
		"operations", "pcl_read", "pcl_stat", "pcl_stat_l", "pcl_write", "read", "readdir", "readlink", "release_lock", 
		"remove", "rename", "renew", "replicate", "rmdir", "secinfo", "setattr", "set_clientidA", "set_clientidB", "statfs", 
		"symlink", "unlock", "write" };
int	a_nfscliv4_size = sizeof(a_nfscliv4) / sizeof(char *);

/* LARGEPAGE */
char	*a_largepage[] = {
	"Freepages", "Usedpages", "Pages", "HighWater", "SizeMB" };


int	a_largepage_size = sizeof(a_largepage)
 / sizeof(char *);

int	lines = 1024;
char	**line;
int	linemax;


#define ARRAYMAX 1024
#define ARRAYWIDTH 128
char	*array[ARRAYMAX];

char	*host = "unknown";
char	time_and_date[1024];

FILE *ufp;
FILE *cfp = NULL;
FILE *gfp = NULL;
FILE *wfp = NULL;


/* This takes a nmon header line full of comma separated data column names and converts it to an array of names */
/* It also strips out characters that upset rrdtool and work around dumb massive wordy AME column names */
/* the result is extra columns don't need to be coded around */
int	names2array(char *s)
{
	int	i;
	int	j;
	int	k;
	int	len;
	len = strlen(s);
	for (i = 0, j = 0; i < len; i++) { /*move to char passed second comma */
		if (s[i] == ',')
			j++;
		if (j == 2)
			break;
	}
	if (j != 2) {
		printf("names2array failure missing 2 commas string=<%s>\n", s);
		return 0;
	}
	i++; /* move past 2nd comma */
	for (j = 0, k = 0; i < len; i++) { /* i = string index, j = array index, k = char index */
		if (s[i] == ',') {
			array[j][k] = 0; /* add null terminator */
			k = 0;
			j++;
		} else if (s[i] == ' ') { /* remove confusing characters */
			continue;
		} else if (s[i] == '(') {
			continue;
		} else if (s[i] == ')') {
			continue;
		} else if (s[i] == '%') {
			continue;
		} else if (s[i] == '-') {
			continue;
		} else {
			array[j][k] = s[i];
			k++;
			array[j][k] = 0; /* add null terminator */
		}
	}
	for (i = 0; i <= j; i++) { /* fix dumb long AME column names */
		len = strlen(array[i]);
		if (len >= 18) {
			/* printf("column name too long = %d \"%s\"\n", len, array[i]); */
			if (strcmp("SizeoftheCompressedpoolMB", array[i]) == 0)
				strcpy(array[i], "AME_Compressed");
			if (strcmp("SizeoftruememoryMB", array[i]) == 0)
				strcpy(array[i], "AME_True");
			if (strcmp("ExpandedmemorysizeMB", array[i]) == 0)
				strcpy(array[i], "AME_Expanded");
			if (strcmp("SizeoftheUncompressedpoolMB", array[i]) == 0)
				strcpy(array[i], "AME_Uncompressed");

			if (strcmp("Compressedpoolpgins", array[i]) == 0)
				strcpy(array[i], "AME_pgins");
			if (strcmp("Compressedpoolpgouts", array[i]) == 0)
				strcpy(array[i], "AME_pgouts");

			if (strcmp("entitled_proc_capacity", array[i]) == 0)
				strcpy(array[i], "entitlement");
			if (strcmp("proc_capacity_increment", array[i]) == 0)
				strcpy(array[i], "proc_increment");
			if (strcmp("unalloc_proc_capacity", array[i]) == 0)
				strcpy(array[i], "unalloc_proc");
			if (strcmp("var_proc_capacity_weight", array[i]) == 0)
				strcpy(array[i], "proc_weight");
			if (strcmp("unalloc_var_proc_capacity_weight", array[i]) == 0)
				strcpy(array[i], "unalloc_weight");
			if (strcmp("online_phys_cpus_sys", array[i]) == 0)
				strcpy(array[i], "online_pcpus_sys");
			if (strcmp("max_phys_cpus_sys", array[i]) == 0)
				strcpy(array[i], "max_pcpus_sys");

			/* printf("\t changed to \"%s\"\n", array[i]); */
		}
	}
	return j + 1;
}


int	str2array(int skip, char *s)
{
	int	i;
	int	j;
	int	k;
	int	len;
	len = strlen(s);
	for (i = 0, j = 0; i < len && j < skip; i++) {
		if (s[i] == ' ')
			j++;
	}
	if (j != skip) {
		printf("str2array skip failure <%s> skip=%d\n", s, skip);
		return 0;
	}
	if (debug)
		printf("str2array str=%s skip to <%s> skip=%d\n", s, &s[i], skip);

	for (j = 0, k = 0; i < len; i++) {
		if (s[i] == ' ') {
			array[j][k] = 0; /* add null terminator */
			k = 0;
			j++;
		} else {
			array[j][k] = s[i];
			k++;
			array[j][k] = 0; /* add null terminator */
		}
	}
	if (s[i-1] != ' ')
		j++;
	return j;
}


void webgraph(char *name)
{
	fprintf(wfp, "<IMG SRC=%s.gif>\n", name);
}


char	*colourmap[] = {
	/* 1*/ "F0F0F0", "FF0000", "00FF00", "0000FF", "FFFF00", "00FFFF", "FF00FF", "0F0F0F",
	/* 2*/ "FF8800", "00FF88", "8800FF", "880000", "008800", "000088", "888800", "008888",
	/* 3*/ "880088", "080808", "884400", "008844", "440088", "888888", "BB0000", "00BB00",
	/* 4*/ "0000BB", "BBBB00", "00BBBB", "BB00BB", "0B0B0B", "BB8800", "00BB88", "8800BB",
	/* 5*/ "BBBBBB", "440000", "004400", "000044", "444400", "004444", "440044", "040404",
	/* 6*/ "448800", "004488", "880044", "444444", "DD0000", "00DD00", "0000DD", "DDDD00",
	/* 7*/ "00DDDD", "DD00DD", "0D0D0D", "DD8800", "00DD88", "8800DD", "DDDDDD", "660000",
	/* 8*/ "006600", "000066", "666600", "006666", "660066", "060606", "668800", "006688",
	/* 9*/ "E0E0E0", "EE0000", "00EE00", "0000EE", "EEEE00", "00EEEE", "EE00EE", "0E0E0E",
	/*10*/ "EE7700", "00EE77", "7700EE", "770000", "007700", "000077", "777700", "007777",
	/*11*/ "770077", "070707", "773300", "007733", "330077", "777777", "AA0000", "00AA00",
	/*12*/ "0000AA", "AAAA00", "00AAAA", "AA00AA", "0A0A0A", "AA7700", "00AA77", "7700AA",
	/*14*/ "AAAAAA", "330000", "003300", "000033", "333300", "003333", "330033", "030303",
	/*15*/ "337700", "003377", "770033", "333333", "CC0000", "00CC00", "0000CC", "CCCC00",
	/*16*/ "00CCCC", "CC00CC", "0C0C0C", "CC7700", "00CC77", "7700CC", "CCCCCC", "550000",
	/*17*/ "005500", "000055", "555500", "005555", "550055", "050505", "557700", "005577",
	/*18*/ "226600", "002266", "660022", "222222", "BB0000", "00BB00", "0000BB", "BBBB00",
	/*19*/ "00BBBB", "BB00BB", "0B0B0B", "BB6600", "00BB66", "6600BB", "BBBBBB", "440000",
	/*20*/ "004400", "000044", "444400", "004444", "440044", "040404", "446600", "004466",
	/*21*/ "880066", "666666", "770055", "555555" };


char	*colour(int col)
{
	if (col > 160)
		return "111111";
	return colourmap[col+1];
}


/* percent variable */
#define AUTO 0		/* opposite of percent = auto scale */
#define PERCENT 1 	/* force vertical scale 0 to 100 */

/* type variable */
#define AREA 2		/* stacked area graph */
#define LINE 3		/* non-stacked lines */

void	rrdgraph(char **fields, int count, char *rrdname, char *gif, int percent, 
char *vtitle, int type, char *units)
{
	int	i;
	int	vars;
	char	*percentstring, *t1, *t2, *stack;

	webgraph(gif);

	if (gfp == NULL) {
		sprintf(filename, "%s/%s", dirname, "rrd_graph");
		gfp = fopen(filename, "w");
		if (gfp == NULL) {
			perror("failed to open file");
			printf("file: \"%s\"\n", filename);
			exit(73);
		}
	}

	if (percent == PERCENT) {
		percentstring = "-r -l 0 -u 100";
	} else {
		percentstring = "-r -l 0";
	}
	if (type == LINE) {
		t1 = "LINE2";
		t2 = "LINE2";
		stack = "";
	} else {
		t1 = "AREA";
		t2 = "STACK";
		stack = " Stacked";
	}
	if (debug)
		fprintf(gfp, "info %s\n", gif);
	sprintf(filename, "%s/%s.gif", dirname, gif);
	if(generic_graph_output) {
		utc_start = 1111;
		utc_end   = 2222;
	}
	fprintf(gfp,
	    "graph %s.gif %s -v \"%s%s\" --start %d --end %d --width %d --height %d --title \"%s %s %s\" ",
	    gif, percentstring, units, stack, utc_start, utc_end, grwidth, grheight, host, vtitle, time_and_date);
	vars = count;

	if (debug)
		for (i = 0; i < vars; i++)
			printf("arr=<%s>\n", fields[i]);

	i = 0;
	fprintf(gfp,
	    "DEF:%s=%s.rrd:%s:AVERAGE %s:%s#%s:\"%s\" ",
	    fields[i], rrdname, fields[i], t1, fields[i], colour(i), fields[i]);
	for (i = 1; i < vars; i++) {
		if (debug)
			printf( "DEF:%s=%s.rrd:%s:AVERAGE %s:%s#%s:\"%s\" \n",
			    fields[i], rrdname, fields[i], t2, fields[i], colour(i), fields[i]);
		fprintf(gfp,
		    "DEF:%s=%s.rrd:%s:AVERAGE %s:%s#%s:\"%s\" ",
		    fields[i], rrdname, fields[i], t2, fields[i], colour(i), fields[i]);
	}
	fprintf(gfp, "\n");
}


void	rrdcreate(char **arr, int vars,  char *rrdname )
{
	int	i;
	if (cfp == NULL) {
		sprintf(filename, "%s/%s", dirname, "rrd_create");
		cfp = fopen(filename, "w");
		if (cfp == NULL) {
			perror("failed to open file");
			printf("file: \"%s\"\n", filename);
			exit(74);
		}
	}

	fprintf(cfp, "create %s.rrd --start %d --step %d  ", rrdname, utc_start, interval);
	for (i = 0; i < vars; i++)
		fprintf(cfp, "DS:%s:GAUGE:%d:U:U ", arr[i], interval * 2);
	fprintf(cfp, " RRA:AVERAGE:0.5:1:%d\n", snapshots * no_of_nmon_files);
}


void	file_io_end()
{
	fclose(cfp);
	fclose(gfp);
	fclose(wfp);
	fclose(ufp);
}


int	founds = 0;
char	**found = (char **)
0;
int	foundmax;

int	findfirst(char *s)
{
	int	i;
	size_t 	len;
	len = strlen(s);
	for (i = 0; i < linemax; i++) {
		if (debug)
			printf("compare <%s> with <%s>\n", s, line[i]);
		if ( strncmp(line[i], s, len) == 0)
			return i;
	}
	return - 1;
}


int	find(char *s)
{
	int	i;
	foundmax = 0;
	for (i = 0; i < linemax; i++) {
		if (debug)
			printf("compare <%s> with <%s>\n", s, line[i]);
		if ( strstr(line[i], s))
			foundmax++;
	}
	if (debug)
		printf("found %s %d times\n", s, foundmax);

	if (founds == 0) {
		if (debug)
			printf("find malloc %d\n", sizeof( char *) * 1024);
		found = (char **)malloc(sizeof( char *) * 1024);
		founds = 1024;
	}
	if (foundmax > founds) {
		if (debug)
			printf("find realloc %d\n", sizeof( char *) * foundmax);
		found = (char **)realloc((void *)found, sizeof(char *) * (foundmax + 1));
		founds = foundmax;
	}
	foundmax = 0;
	for (i = 0; i < linemax; i++)
		if ( strstr(line[i], s)) {
			found[foundmax] = line[i];
			foundmax++;
		}
	return foundmax;
}






void	run(char *cmd)
{
	printf("%s\n", cmd);
	system(cmd);
}


void hint()
{
	printf("nmon2rrd -f nmonfile [-d directory] [-x] [-n files] [-g] [-w width] [-h height]\t\tVersion:%s\n", VERSION);
	printf(" -f nmonfile    the regular CSV nmon output file\n");
	printf(" -d directory   dirname for the output\n");
	printf(" -w width       graph width  (default 800, max 1500)\n");
	printf(" -h height      graph height (default 300)\n");
	printf(" -x             execute the output files\n");
	printf(" -n files       make the rrdtool database larger for this number of nmon files (effects the rrd_create file)\n");
	printf(" -g             change the rrd_graph file to make it easier to change \"--start 0000001111 --end 0000002222\"\n");
	printf("                extract the actual start & end seconds: rrdtool first net.rrd ;  rrdtool last net.rrd\n");
	printf(" -b             change rrd_graph script dates to YYYY/MM/DD\n");
	printf("Example:\n");
	printf(" nmon2rrd -f m1_030811_1534.nmon -d /webpages/docs/m1/030811 -x \n");


	printf("  - or - \n");
	printf(" nmon2rrd -f my.nmon -d /home/nag/tmp42\n");
	printf(" Manually complete with:\n");
	printf(" cd /home/nag/tmp42\n");
	printf(" rm -f *.rrd\n");
	printf(" rm -f *.gif\n");
	printf(" rrdtool - < rrd_create\n");
	printf(" rrdtool - < rrd_update >rrd_update.log\n");
	printf(" rrdtool - < rrd_top >rrd_top.log\n");
	printf(" rrdtool - < rrd_graph\n");
	printf(" then copy the *.gif and index.html files to your web server\n");

	exit(42);
}


int	main(int argc, char **argv)
{
	int	i;
	int	j;
	int	k;
	int	ret;
	int	len;
	char	*s;
	int	cpus;
	int	cpus2;
	int	thour, tmins, tsecs, tnum;
	float	busy = 0.0;
	int	hour, mins, secs;
	int	day, month, year;
	int	n;
	struct tm *timp;
	time_t timt;
	char	string1[1024];
	char	string2[1024];
	int	missing;
	int	disksects;
	int	disksect_found;
	char	*progname;
	char	*nmonversion = "unknown";
	char	*user = "unknown";
	char	*runname = "unknown";
	char	*rundate = "unknown";
	char	*hardware = "unknown";
	char	*kernel = "unknown";
	char	*aix = "unknown";
	char	*aixtl = "unknown";
	char         *OS="unknown";
	char         *arch="unknown";
	int	a_net_size;
	int	a_wlm_size;
	int	a_paging_size;
	int	a_neterr_size;
	int	neterror_found = 0;
	int	a_jfs_size;
	int	a_ess_size;
	int	a_dg_size;
	int	a_ioa_size;
	int	a_fc_size;
	char	**a_net;
	char	**a_wlm;
	char	**a_paging;
	char	**a_neterr;
	char	**a_jfs;
	char	**a_jfsdummy;
	char	**a_ess;
	char	**a_dg;
	char	**a_ioa;
	char	**a_fc;
	int	a_disk_size[151];
	char	**a_disk[151];
	FILE * fp;
	int	file_arg = 1;
	int	execute = 0;
	int	top_found = 0;
	int	ess_found = 0;
	int	adapt_found = 0;
	int	fc_found = 0;
	int	dg_found = 0;
	int	sanity = 55;
	int	aio_found = 0;
	int	lpar_found = 0;
	int	netpacket_found = 0;
	int	nfs_found = 0;
	int	nfscliv2_found = 0;
	int	nfscliv3_found = 0;
	int	nfscliv4_found = 0;
	int	nfssvrv2_found = 0;
	int	nfssvrv3_found = 0;
	int	nfssvrv4_found = 0;
	int	wlm_found = 0;
	int	paging_found = 0;
	int	largepage_found = 0;
	int	short_style = 0;
	int	nmon9 = 0;
	int	nmon12e = 0;
	int	nmon11e = 0;
	int	nmon11d = 0;
	int	topas_nmon = 0;
	char	*infile = NULL;

	if (getenv("NMON2RRDDEBUG") != 0)
		debug++;

	for (i = 0; i < ARRAYMAX; i++)
		array[i] = malloc(ARRAYWIDTH);

	line = (char **)malloc(sizeof(char *) * 1024);

	while ( -1 != (i = getopt(argc, argv, "?f:d:xw:h:n:gb" ))) {
		switch (i) {
		case '?':
			hint();
			exit(0);
			break;
		case 'x':
			execute++;
			break;
		case 'w':
			sscanf(optarg, "%d", &grwidth);
			break;
		case 'h':
			sscanf(optarg, "%d", &grheight);
			break;
		case 'f':
			infile = optarg;
			break;
		case 'd':
			strcpy(dirname, optarg);
			break;
		case 'n':
			no_of_nmon_files = atoi(optarg);
			if(no_of_nmon_files < 1) no_of_nmon_files=1;
			printf("Multiplying rrdtool records by %d (-n option)\n",no_of_nmon_files);
			break;
		case 'g':
			generic_graph_output++;
			break;
		case 'b':
			backward=1;
			printf("Using date format YYYY/MM/DD\n");
			break;
		}
	}
	if (infile == NULL) {
		printf("Error: nmon filename missing\n");
		hint();
	}
	if ( (fp = fopen(infile, "r")) == NULL) {
		perror("failed to open file");
		printf("file: \"%s\"\n", infile);
		exit(75);
	}

	for (i = 0; fgets(string, STRLEN, fp) != NULL; i++) {
		if (i >= lines) {
			lines += 1024;
			line = (char **)realloc((void *)line, sizeof(char *) * lines);
		}
		if (string[strlen(string)-1] == '\n')
			string[strlen(string)-1] = 0;
		if (string[strlen(string)-1] == '\r')
			string[strlen(string)-1] = 0;
		if (string[strlen(string)-1] == ' ')
			string[strlen(string)-1] = 0;
		if (string[strlen(string)-1] == ',')
			string[strlen(string)-1] = 0;
		len = strlen(string) + 1;
		if (len > longest)
			longest = len;
		s = malloc(len);
		strcpy(s, string);
		line[i] = (char *)s;
	}
	linemax = i;
	lines = i;

	if (debug)
		for (i = 0; i < linemax; i++)
			printf("line %d lastline %s\n", i, line[i-1]);
	//Linux mode
       if ( (n = findfirst("AAA,OS,Linux")) != -1){
		linuxone=1; 
                OS=&line[n][1];
                printf("os line %s\n",OS);
                 if(strstr(OS,"s390x"))
			 arch = "s390x";
		         OS="Linux";
		 
		 if(strstr(OS,"x86_64"))
			 arch="x86 64Bit";
		         OS="Linux";
	 };
	printf("LinuxMode");

	printf("nmon2rrd version %s\n", VERSION);
	n = findfirst("AAA,progname");
	if (n == -1) {
		printf("ERROR: This does not appear to be regular nmon capture file\n");
		printf("ERROR: Can't find line starting \"AAA,progname\"\n");
		printf("ERROR: nmon2rrd does NOT use the rrd nmon output format\n");
		exit(33);
	}
	progname = &line[n][13];
	printf("progname=%s\n", progname);

	if ( (n = findfirst("AAA,version")) != -1)
		/* nmonversion=&line[n][13]; */
		nmonversion = &line[n][12];
	printf("nmonversion line=%s\n", &line[n][0]);
	printf("nmonversion=%s\n", nmonversion);
	if ( nmonversion[0] == '5')
		short_style = 1;
	if ( nmonversion[0] == '9')
		nmon9 = 1;

	if ( nmonversion[0] == '1' && 
	    nmonversion[1] == '1' && 
	    nmonversion[2] == 'd' ) {
		nmon11d = 1;
	}

	if ( nmonversion[0] == '1' && 
	    nmonversion[1] == '1' && 
	    nmonversion[2] == 'e' ) {
		nmon11e = 1;
	}

	if ( nmonversion[0] == '1' && 
	    nmonversion[1] == '2' && 
	    nmonversion[2] == 'e' ) {
		nmon12e = 1;
	}

	if ( nmonversion[0] == 'T' && 
	    nmonversion[1] == 'O' && 
	    nmonversion[2] == 'P' && 
	    nmonversion[3] == 'A' && 
	    nmonversion[4] == 'S' && 
	    nmonversion[5] == '-' && 
	    nmonversion[6] == 'N' && 
	    nmonversion[7] == 'M' && 
	    nmonversion[8] == 'O' && 
	    nmonversion[9] == 'N' ) {
		topas_nmon = 1;
		if (debug) {
			printf("DEBUG ****** TOPAS-NMON ******\n");
			printf("DEBUG ****** nmon12e == %d ******\n", nmon12e);
		}
	}

	/* AAA,host,aix10 */
	if ( (n = findfirst("AAA,host")) != -1)
		host = &line[n][9];
	printf("host=%s\n", host);

	/* AAA,user,root */
	if ( (n = findfirst("AAA,user")) != -1)
		user = &line[n][9];
	printf("user=%s\n", user);

	/* AAA,runname,aix10 */
	if ( (n = findfirst("AAA,runname")) != -1)
		runname = &line[n][12];
	printf("runname=%s\n", runname);

	/* AAA,rundate (note that 10 is like $left() */
	if ( (n = findfirst("AAA,date")) != -1)
		rundate = &line[n][9];
	printf("rundate=%s\n", rundate);

	/* AAA,AIX,4.3.3.84 */
	if ( (n = findfirst("AAA,AIX")) != -1)
		aix = &line[n][8];
	printf("aix=%s\n", aix);

	/* AAA,TL,6 */
	if ( (n = findfirst("AAA,TL")) != -1)
		aixtl = &line[n][7];
	printf("aixtl=%s\n", aixtl);


	/* AAA,hardware,XXX */
	if ( (n = findfirst("AAA,hardware")) != -1)
		hardware = &line[n][13];
	printf("hardware=%s\n", hardware);

	/* AAA,kernel,XXX */
	if ( (n = findfirst("AAA,kernel")) != -1)
		kernel = &line[n][11];
	printf("kernel=%s\n", kernel);

	/* AAA,interval,10 */
	if ( (n = findfirst("AAA,interval")) != -1)
		sscanf(line[n], "AAA,interval,%d", &interval);
	else {
		printf("Warning: Interval line \"AAA,interval\" missing\n");
		exit(3245);
	}
	printf("interval=%d\n", interval);

	/* AAA,snapshots,300 */
	if ( (n = findfirst("AAA,snapshots")) != -1) {
		sscanf(line[n], "AAA,snapshots,%d", &snapshots);
		printf("snapshots=%d\n", snapshots);
		snapshots++; /* make sure rrd does not compress data */
	} else {
		printf("Warning: snapshots line \"AAA,snapshots\" missing\n");
		snapshots = 1;
	}
	if (snapshots > 1500) {
		printf("WARNING: truncating to 1440 snapshots (not the %d found in the nmon file)\n", snapshots);
		snapshots = 1440;
	}

	/* AAA,cpus,12 */
	if ( (n = findfirst("AAA,cpus")) != -1){
		 if(linuxone==1){
		       cpus=cpus2=0;}
		sscanf(line[n], "AAA,cpus,%d,%d", &cpus2, &cpus);
	        if(cpus==0){
			cpus=cpus2;}}
	else {
		printf("WARNING: missing \"AAA,cpus\" line assuming 1\n");
		cpus = 1;
		if(linuxone==1){
			cpus=cpus2;}
	}

	printf("cpus=%d online out of a maximum of %d\n", cpus, cpus2);

	if (findfirst("LPAR,Logical") != -1)
		lpar_found = 1;

	if (findfirst("PROCAIO,T") != -1)
		aio_found = 1;
	if (findfirst("NETPACKET,T") != -1)
		netpacket_found = 1;
	if (findfirst("NFSSVRV2,T") != -1)
		nfs_found = 1;
	if (findfirst("LARGEPAGE,T") != -1)
		largepage_found = 1;

	if (findfirst("DISKRSIZE,T") != -1)
		printf("Warning: ignoring DISKRSIZE lines - data not reliable\n");
	if (findfirst("DISKWSIZE,T") != -1)
		printf("Warning: ignoring DISKWSIZE lines - data not reliable\n");


	/* AAA,time,09:19.48 */
	if ( (n = findfirst("AAA,time")) != -1) {
		sscanf(line[n], "AAA,time,%d:%d.%d", &hour, &mins, &secs);
		printf("hour=%d minutes=%d seconds=%d\n", hour, mins, secs);
	}

	/* AAA,date,22/04/03 */
	/* and now AAA,date,22-JAN-2003 */
	if ( (n = findfirst("AAA,date")) != -1) {
		ret = sscanf(line[n], "AAA,date,%d/%d/%d", &day, &month, &year);
		if (ret != 3) {
			ret = sscanf(&line[n][9], "%d", &day);
			ret = sscanf(&line[n][16], "%d", &year);
			year -= 2000;
			for (i = 0; i < 12; i++) {
				if (!strncmp(&line[n][12], months[i], 3)) {
					month = i + 1;
					break;
				}
			}
		}
	} else {
		printf("Error: Missing \"AAA,date\" line - aborting\n");
		exit(6335);
	}
	printf("day=%d month=%d year=%d\n", day, month, year);

	timt = time(0);
	timp = localtime(&timt);
	if (debug)
		printf("%d:%02d.%02d %02d/%02d/%04d = %ld\n",
		    timp->tm_hour,
		    timp->tm_min,
		    timp->tm_sec,
		    timp->tm_mday,
		    timp->tm_mon + 1,
		    timp->tm_year + 1900, timt);
	timp->tm_sec = secs;
	timp->tm_min = mins;
	timp->tm_hour = hour;
	timp->tm_mday = day;
	timp->tm_mon = month - 1;
	timp->tm_year = year + 100;

	timt = mktime(timp);
	if (debug)
		printf("%d:%02d.%02d %02d/%02d/%04d = %ld\n",
		    timp->tm_hour,
		    timp->tm_min,
		    timp->tm_sec,
		    timp->tm_mday,
		    timp->tm_mon + 1,
		    timp->tm_year + 1900, timt);
	if (debug)
		printf("%ld\n", timt);
	if(backward) {
	sprintf(time_and_date, "%d:%02d.%02d %04d/%02d/%02d",
	    timp->tm_hour,
	    timp->tm_min,
	    timp->tm_sec,
	    timp->tm_year + 1900,
	    timp->tm_mon + 1,
	    timp->tm_mday);
	} else {
	sprintf(time_and_date, "%d:%02d.%02d %02d/%02d/%04d",
	    timp->tm_hour,
	    timp->tm_min,
	    timp->tm_sec,
	    timp->tm_mday,
	    timp->tm_mon + 1,
	    timp->tm_year + 1900);
	}

	/* process ZZZZ sections */
	n = find("ZZZZ,T");
	if ( n < snapshots - 1 ) {
		printf("Warning: actual snapshots=%d is less than the requested=%d so nmon finished early\n", n, snapshots);
		snapshots = n;
		utc_end   = (int)timt + interval * (snapshots + 1);
	}
	for (i = 0; i < n; i++) {
		if ( (sscanf(found[i], "ZZZZ,T%d,%d:%d:%d", &tnum, &thour, &tmins, &tsecs)) != 4) {
			printf("Error: invalid ZZZZ line (%d) \"%s\"\n", i, found[i]);
			exit(529);
		}
		if (debug) {
			printf("found T=T%04d %02d %02d %02d", tnum, thour, tmins, tsecs);
			printf(" %02d %02d %02d\n", day, month, year);
		}

		tarray[tnum] = utc(tsecs, tmins, thour, day, month - 1, year + 100);
		/* check for wrap around of day */
		if (i > 0 && tarray[tnum] < tarray[tnum-1]) {
			day++;
			tarray[tnum] = utc(tsecs, tmins, thour, day, month - 1, year + 100);
		}

		if (debug)
			printf("utc=%ld\n", tarray[tnum]);
	}

	/*
   Set utc_start and utc_end based on the actual data collection times.
   They are used for the graphs' x-axis start and end times.
*/
	utc_start = tarray[1] - 1;
	utc_end = tarray[tnum];

	if (no_of_nmon_files == 1) 
		start_top(snapshots);
	top_found = n = find("TOP,");
	if (top_found && no_of_nmon_files == 1 ) {
		for (k = 0; k < n; k++) {
			if (isdigit(found[k][4])) {
				if ( (i = search_for_tstring(found[k])) != 0) {
					tnum = -1;
					busy = -1.0;
					if ( sscanf(&found[k][i+1], "%d,%f", &tnum, &busy) != 2) {
						printf("Error: invalid TOP line (%d) \"%s\"\n", i, found[k]);
						exit(738);
					}
					if (tnum == -1 || busy == -1.0) {
						printf("nmon2rrd: top section scanf failed - %s\n", string);
						continue;
					}

					for (i = i + 5; i < strlen(found[k]); i++) {
						if (isalpha(found[k][i])) {
							for (j = i; j < strlen(found[k]); j++) {
								if (found[k][j] == ',')
									found[k][j] = 0;
							}
							save_top(tnum, busy, &found[k][i]);
							break;
						}
					}
				}
			}
		}
		end_top();
	}

	if ( (n = findfirst("WLMCPU,T000")) != -1) {
		if (strlen(line[n]) > 14) { /* found some examples with no data */
			if ( (n = findfirst("WLMCPU,CPU")) == -1) {
				wlm_found = 0;
			} else {
				wlm_found = 1;
				printf("WLM stats found\n");
				replace1(line[n], " ", "");
				replace1(line[n], ".", "");
				replace1(line[n], ",", " ");

				a_wlm_size = str2array(2, line[n]);
				if (debug)
					for (i = 0; i < a_wlm_size; i++)
						printf("WLM class names are = %s\n", array[i]);
				a_wlm = malloc(sizeof(char *) * a_wlm_size);
				for (i = 0; i < a_wlm_size; i++) {
					a_wlm[i] = malloc(strlen(array[i]) + 1 + 5);
					strcpy(a_wlm[i], array[i]);
				}
			}
		}
	}

	if ( (n = findfirst("PAGING,PagingSpace")) == -1) {
		paging_found = 0;
	} else {
		paging_found = 1;
		replace1(line[n], " ", "");
		replace1(line[n], ",", " ");

		a_paging_size = str2array(2, line[n]);
		if (debug)
			for (i = 0; i < a_paging_size; i++)
				printf("Paging Space names are = %s\n", array[i]);
		a_paging = malloc(sizeof(char *) * a_paging_size);
		for (i = 0; i < a_paging_size; i++) {
			a_paging[i] = malloc(strlen(array[i]) + 1 + 5);
			strcpy(a_paging[i], array[i]);
		}
	}

	if ( (n = findfirst("NET,Network")) == -1) {
		printf("Error: no network line found\n");
		exit(562);
	}
	replace1(line[n], " ", "");
	replace1(line[n], ",", " ");
	replace1(line[n], "-read-KB/s", "");
	replace1(line[n], "-read-kB/s", "");

	a_net_size = str2array(2, line[n]);
	if (debug)
		for (i = 0; i < a_net_size / 2; i++)
			printf("networknames are = %s\n", array[i]);
	a_net = malloc(sizeof(char *) * a_net_size);
	for (i = 0; i < a_net_size / 2; i++) {
		a_net[i] = malloc(strlen(array[i]) + 1 + 5);
		strcpy(a_net[i], array[i]);
		strcat(a_net[i], "_read");

		a_net[i+a_net_size/2] = malloc(strlen(array[i]) + 1 + 6);
		strcpy(a_net[i+a_net_size/2], array[i]);
		strcat(a_net[i+a_net_size/2], "_write");
	}
	if (debug)
		for (i = 0; i < a_net_size; i++)
			printf("net are = %s\n", a_net[i]);

	/* TM 06/02/2006 */
	if ( (n = findfirst("NETERROR,Network")) == -1) {
		printf("Warning: no network error line found\n");
	} else {
		neterror_found++;
		replace1(line[n], " ", "");
		replace1(line[n], ",", " ");
		replace1(line[n], "-ierrs", "_inp");
		replace1(line[n], "-oerrs", "_out");
		replace1(line[n], "-collisions", "_col");
		a_neterr_size = str2array(2, line[n]);
		a_neterr = malloc(sizeof(char *) * a_neterr_size);
		for (i = 0; i < a_neterr_size; i++) {
			a_neterr[i] = malloc(strlen(array[i]) + 1);
			strcpy(a_neterr[i], array[i]);
		}
	}

	n = findfirst("JFSFILE,JFS");
	replace1(line[n], " ", "");
	replace1(line[n], ",", " ");

	a_jfs_size = str2array(2, line[n]);
	a_jfs = malloc(sizeof(char *) * (a_jfs_size));
	a_jfsdummy = malloc(sizeof(char *) * (a_jfs_size));
	for (i = 0; i < a_jfs_size; i++) {
		a_jfs[i] = malloc(strlen(array[i]) + 1);
		strcpy(a_jfs[i], array[i]);
		a_jfsdummy[i] = malloc(6);
		sprintf(a_jfsdummy[i], "fs%03d", i + 1);
	}
	if (debug)
		for (i = 0; i < a_jfs_size; i++)
			printf("jfs%3d = %s\n", i, a_jfs[i]);

	for (disksect_found = 1, i = 1; disksect_found != -1; i++) {
		sprintf(string1, "DISKBUSY%d", i);
		/*printf("looking for %s\n",string1); */
		disksect_found = findfirst(string1);
		/*printf("missing=%d\n",disksect_found) */;
	}
	disksects = i - 1;
	printf("Found %d DISKBUSY Section(s)\n", disksects);

	for (j = 0; j < disksects; j++) {
		if (j == 0)
			strcpy(string1, "DISKBUSY,");
		else
			sprintf(string1, "DISKBUSY%d", j);
		n = findfirst(string1);
		replace1(line[n], "%", "");
		replace1(line[n], " ", "");
		replace1(line[n], "-", "");
		replace1(line[n], ",", " ");
		a_disk_size[j] = str2array(2, line[n]);
#define MAXDISK 150
		if (a_disk_size[j] > MAXDISK ) {
			printf("Warning: More than %d disks in this DISK section.\n\tThis can not shown on a single graph, so above %d disks ignored.\n",
			     			     MAXDISK, MAXDISK);
			a_disk_size[j] = MAXDISK - 1;
		}
		a_disk[j] = malloc(sizeof(char *) * (a_disk_size[j]));
		for (i = 0; i < a_disk_size[j]; i++) {
			a_disk[j][i] = malloc(strlen(array[i]) + 1);
			strcpy(a_disk[j][i], array[i]);
		}
		if (debug)
			for (i = 0; i < a_disk_size[j]; i++)
				printf("disk%d are = %s\n", j, a_disk[j][i]);
	}

	n = findfirst("DGBUSY,Disk");
	if (n != -1)
		dg_found = 1;
	if (dg_found) {
		if (debug)
			printf("DG <%s>\n", line[n]);
		replace1(line[n], " ", "");
		replace1(line[n], "-", "");
		replace1(line[n], ",", " ");
		if (debug)
			printf("replaced <%s>\n", line[n]);
		a_dg_size = str2array(2, line[n]);
		if (debug)
			printf("size=%d\n", a_dg_size);
		a_dg = malloc(sizeof(char *) * (a_dg_size));
		for (i = 0; i < a_dg_size; i++) {
			a_dg[i] = malloc(strlen(array[i]) + 1);
			strcpy(a_dg[i], array[i]);
		}
		if (debug)
			for (i = 0; i < a_dg_size; i++)
				printf("dg are = %s\n", a_dg[i]);
		if(a_dg_size==0)
			dg_found=0;
	}
	n = findfirst("ESSREAD,ESS");
	if (n != -1)
		ess_found = 1;
	if (ess_found) {
		if (debug)
			printf("ESS <%s>\n", line[n]);
		replace1(line[n], "KB/s", "");
		replace1(line[n], " ", "");
		replace1(line[n], "-", "");
		replace1(line[n], ",", " ");
		if (debug)
			printf("replaced <%s>\n", line[n]);
		a_ess_size = str2array(2, line[n]);
		if (a_ess_size > ARRAYMAX ) {
			printf("Warning: More than 128 vpaths!\n\tThis can not really be shown on a single graph above 160 all have the same colour.\n",
			     			     ARRAYMAX);
			a_ess_size = ARRAYMAX;
		}
		if (debug)
			printf("size=%d\n", a_ess_size);
		a_ess = malloc(sizeof(char *) * (a_ess_size));
		for (i = 0; i < a_ess_size; i++) {
			a_ess[i] = malloc(strlen(array[i]) + 1);
			strcpy(a_ess[i], array[i]);
		}
		if (debug)
			for (i = 0; i < a_ess_size; i++)
				printf("ess are = %s\n", a_ess[i]);
	}
	/* Fibre Channel */
        n = findfirst("FCREAD,");
        if (n != -1)
                fc_found = 1;
        if (fc_found) {
                replace1(line[n], " ", "");
                replace1(line[n], ",", " ");
                replace1(line[n], "KB/s", "");
                replace1(line[n], "kB/s", "");
                a_fc_size = str2array(2, line[n]);
                a_fc = malloc(sizeof(char *) * (a_fc_size));
                for (i = 0; i < a_fc_size; i++) {
                        a_fc[i] = malloc(strlen(array[i]) + 1);
                        strcpy(a_fc[i], array[i]);
                }
                if (debug)
                        for (i = 0; i < a_fc_size; i++)
                                printf("fc are = %s\n", a_fc[i]);
        }

	n = findfirst("IOADAPT,Disk");
	if (n != -1)
		adapt_found = 1;
	if (adapt_found) {
		replace1(line[n], " ", "");
		replace1(line[n], ",", " ");
		replace1(line[n], "-KB/s", "");
		replace1(line[n], "-kB/s", "");
		replace1(line[n], "xfer-", "");
		a_ioa_size = str2array(2, line[n]);
		a_ioa = malloc(sizeof(char *) * (a_ioa_size));
		for (i = 0; i < a_ioa_size; i++) {
			a_ioa[i] = malloc(strlen(array[i]) + 1);
			strcpy(a_ioa[i], array[i]);
		}
		if (debug)
			for (i = 0; i < a_ioa_size; i++)
				printf("ioadapt are = %s\n", a_ioa[i]);
	} else {
		if (debug)
			printf("ioadapt not found\n");
	}
        if(linuxone==1){
		rrdcreate(l_cpu,l_cpu_size,"cpu_all");
		if (cpus >1 )
		for (i = 1; i <= cpus; i++) {
			sprintf(string2, "cpu%03d", i);
			rrdcreate(l_cpu, l_cpu_size, string2);
		}

	}else{
	rrdcreate(a_cpu, a_cpu_size , "cpu_all");
		if (cpus > 1)
		for (i = 1; i <= cpus; i++) {
			sprintf(string2, "cpu%02d", i);
			rrdcreate(a_cpu, a_cpu_size, string2);
		}

        }
	
	if ( (n = findfirst("LPAR,Logical")) != -1) {
	int comma1;
	int comma2;
		i = names2array(line[n]);
		for(k=comma1=0;line[n][k] != 0;k++)
			if(line[n][k] == ',') comma1++;	
		if (debug)
			for (j = 0; j < i; j++)
				printf("14 LPAR[%d]=%s\n", j, array[j]);
		/* Work arround topas_nmon bug with Pool_id in the header but not in the data */
		if ( (n = findfirst("LPAR,T0001")) != -1) {
			for(j=comma2=0;line[n][j] != 0;j++)
				if(line[n][j] == ',') comma2++;	
		}
		printf("comma1=%d comma2=%d\n",comma1, comma2);
		
		/* resync the array */
		n = findfirst("LPAR,Logical");
		i = names2array(line[n]);
		if (comma1 != comma2) i--;
	
		rrdcreate(array, i, "lpar");
	}

	if ( (n = findfirst("MEM,")) != -1) {
		i = names2array(line[n]);
		mem_size = i;
		if (debug)
			for (j = 0; j < i; j++)
				printf("14 MEM[%d]=%s\n", j, array[j]);
		rrdcreate(array, i, "mem");
	}

	if ( (n = findfirst("MEMUSE,")) != -1) {
		i = names2array(line[n]);
		memuse_size = i;
		if (debug)
			for (j = 0; j < i; j++)
				printf("14 MEMUSE[%d]=%s\n", j, array[j]);
		rrdcreate(array, i, "memuse");
	}

	if ( (n = findfirst("MEMNEW,")) != -1) {
		i = names2array(line[n]);
		memnew_size = i;
		if (i >= 7) {
			ame_found = 1;
			printf("AME data found\n");
		}
		if (debug)
			for (j = 0; j < i; j++)
				printf("14 MEMNEW[%d]=%s\n", j, array[j]);
		rrdcreate(array, i, "memnew");
	}

	if ( (n = findfirst("PROC,")) != -1) {
		i = names2array(line[n]);
		if (debug)
			for (j = 0; j < i; j++)
				printf("14 PROC[%d]=%s\n", j, array[j]);
		rrdcreate(array, i, "proc");
	}
        if(linuxone==0){
        if ( ( n= findfirst("VM,"))!=-1){
		i=names2array(line[n]);
		for (j = 0; j < i; j++)
			printf("14 vm[%d]=%s\n", j, array[j]);
		rrdcreate(array,i,"vm");
	}
	}	
	if ( (n = findfirst("PAGE,")) != -1) {
		i = names2array(line[n]);
		page_size = i;
		if (debug)
			for (j = 0; j < i; j++)
				printf("14 PAGE[%d]=%s\n", j, array[j]);
		rrdcreate(array, i, "page");
	}
	if (largepage_found)
		rrdcreate(a_largepage, a_largepage_size, "largepage");

	if (aio_found == 1)
		rrdcreate(a_aio, a_aio_size, "procaio");

	rrdcreate(a_file, a_file_size, "file");
	rrdcreate(a_net, a_net_size, "net");
	if (wlm_found) {
		rrdcreate(a_wlm, a_wlm_size, "wlmcpu");
		rrdcreate(a_wlm, a_wlm_size, "wlmmem");
		rrdcreate(a_wlm, a_wlm_size, "wlmbio");
	}
	if (paging_found)
		rrdcreate(a_paging, a_paging_size, "paging");

	/* TM 06/02/2006 */
	if (neterror_found)
		rrdcreate(a_neterr, a_neterr_size, "neterror");

	if (netpacket_found == 1)
		rrdcreate(a_net, a_net_size, "netpacket");

	if ( (n = findfirst("NFSCLIV2,")) != -1) {
		rrdcreate(a_nfscliv2, a_nfscliv2_size, "nfscliv2");
		nfscliv2_found=1;
	}
	if ( (n = findfirst("NFSCLIV3,")) != -1) {
		rrdcreate(a_nfscliv3, a_nfscliv3_size, "nfscliv3");
		nfscliv3_found=1;
	}
	if ( (n = findfirst("NFSCLIV4,")) != -1) {
		rrdcreate(a_nfscliv4, a_nfscliv4_size, "nfscliv4");
		nfscliv4_found=1;
	}
	if ( (n = findfirst("NFSSVRV2,")) != -1) {
		rrdcreate(a_nfssvrv2, a_nfssvrv2_size, "nfssvrv2");
		nfssvrv2_found=1;
	}
	if ( (n = findfirst("NFSSVRV3,")) != -1) {
		rrdcreate(a_nfssvrv3, a_nfssvrv3_size, "nfssvrv3");
		nfssvrv3_found=1;
	}
	if ( (n = findfirst("NFSSVRV4,")) != -1) {
		rrdcreate(a_nfssvrv4, a_nfssvrv4_size, "nfssvrv4");
		nfssvrv4_found=1;
	}

	for (j = 0; j < disksects; j++) {
		sprintf(string2, j ? "diskbusy%d" : "diskbusy", j);
		rrdcreate(a_disk[j], a_disk_size[j], string2);
		sprintf(string2, j ? "diskread%d" : "diskread", j);
		rrdcreate(a_disk[j], a_disk_size[j], string2);
		sprintf(string2, j ? "diskwrite%d" : "diskwrite", j);
		rrdcreate(a_disk[j], a_disk_size[j], string2);
		sprintf(string2, j ? "diskxfer%d" : "diskxfer", j);
		rrdcreate(a_disk[j], a_disk_size[j], string2);
		sprintf(string2, j ? "diskbsize%d" : "diskbsize", j);
		rrdcreate(a_disk[j], a_disk_size[j], string2);
	}
	rrdcreate(a_jfsdummy, a_jfs_size, "jfsfile");
	rrdcreate(a_jfsdummy, a_jfs_size, "jfsinode");
	if (fc_found) {
		rrdcreate(a_fc, a_fc_size, "fcread");
		rrdcreate(a_fc, a_fc_size, "fcwrite");
		rrdcreate(a_fc, a_fc_size, "fcxferin");
		rrdcreate(a_fc, a_fc_size, "fcxferout");
	}
	if (adapt_found) {
		rrdcreate(a_ioa, a_ioa_size, "ioadapt");
	}

	if (dg_found) {
		rrdcreate(a_dg, a_dg_size, "dgbusy");
		rrdcreate(a_dg, a_dg_size, "dgread");
		rrdcreate(a_dg, a_dg_size, "dgwrite");
		rrdcreate(a_dg, a_dg_size, "dgsize");
		rrdcreate(a_dg, a_dg_size, "dgxfer");
	}
	if (ess_found) {
		rrdcreate(a_ess, a_ess_size, "essread");
		rrdcreate(a_ess, a_ess_size, "esswrite");
		rrdcreate(a_ess, a_ess_size, "essxfer");
	}


	/* webhead */
	sprintf(filename, "%s/%s", dirname, "index.html");
	if ( (wfp = fopen(filename, "w")) == NULL) {
		perror("failed to open file");
		printf("file: \"%s\"\n", filename);
		exit(75);
	}
            
	if(backward) {
		fprintf(wfp, "<HTML><TITLE>%s %04d/%02d/%02d %02d:%02d.%02d</TITLE><BODY>\n", 
			host, 2000 + year, month, day, hour, mins, secs);
		fprintf(wfp, "<H1>%s %04d/%02d/%02d %02d:%02d.%02d</H1>\n",
		    host, 2000 + year, month, day, hour, mins, secs);
	} else {
		fprintf(wfp, "<HTML><TITLE>%s %02d/%02d/%04d %02d:%02d.%02d</TITLE><BODY>\n", 
			host, day, month, 2000 + year, hour, mins, secs);
		fprintf(wfp, "<H1>%s %02d/%02d/%04d %02d:%02d.%02d</H1>\n",
		    host, day, month, 2000 + year, hour, mins, secs);
	}
	fprintf(wfp, "<OL>\n");
        fprintf(wfp,"<form action=\"info.php\" method=\"get\"><input type=\"submit\" value=\"Refresh\"></form>");
	if(backward) {
		fprintf(wfp, "<LI>Creation Date: %02d/%02d/%02d\n", 2000 +  year, month, day);
	} else {
		fprintf(wfp, "<LI>Creation Date: %02d/%02d/%02d\n", day, month, 2000 + year);
	}
	fprintf(wfp, "<LI>Start Time: %02d:%02d.%02d\n", hour, mins, secs);
	fprintf(wfp, "<LI>Run Date from the file: %s\n", rundate);
	if(linuxone==0){
	fprintf(wfp, "<LI>AIX: %s\n", aix);
	fprintf(wfp, "<LI>AIX TL: %s\n", aixtl);}
	else{
	fprintf(wfp,"<LI>Operating System: %s\n",OS);	
	}
	fprintf(wfp, "<LI>Runname: %s\n", runname);
	fprintf(wfp, "<LI>Interval: %d\n", interval);
	fprintf(wfp, "<LI>Snapshots: %d\n", snapshots);
	if(linuxone==0){
	fprintf(wfp, "<LI>Hardware: %s\n", hardware);
	fprintf(wfp, "<LI>Kernel: %s\n", kernel); }
	else{
	fprintf(wfp,"<LI>Hardware: %s\n",arch);	
	}
	fprintf(wfp, "<LI>CPU (start/now or max): %d/%d\n", cpus, cpus2);
	fprintf(wfp, "<LI>Version of nmon: %s\n", nmonversion);
	fprintf(wfp, "<LI>Version of nmon2rrd: %s\n", VERSION);
	fprintf(wfp, "<LI>User: %s\n", user);
	fprintf(wfp, "<LI>AME: %s\n", ame_found ? "yes" : "no");
	fprintf(wfp, "</OL>\n");

	


	
	if(linuxone==1){
		rrdgraph(l_cpu, l_cpu_size, "cpu_all", "cpu_all", PERCENT,  "Overall CPU Utilisation - CPU_ALL", AREA, "Percent");
		fprintf(wfp, "<BR>\n");
		if (cpus > 1)
		for (i = 1; i <= cpus; i++) {
			sprintf(string1, "CPU number %03d Utilisation", i);
			sprintf(string2, "cpu%03d", i);
			rrdgraph(l_cpu,l_cpu_size, string2, string2, PERCENT, string1 , AREA, "Percent");
		}	
	}else{
		
		rrdgraph(a_cpu, a_cpu_size, "cpu_all", "cpu_all", PERCENT,  "Overall CPU Utilisation - CPU_ALL", AREA, "Percent");
                fprintf(wfp, "<BR>\n");	
		if (cpus > 1)
		for (i = 1; i <= cpus; i++) {
			sprintf(string1, "CPU number %02d Utilisation", i);
			sprintf(string2, "cpu%02d", i);
			rrdgraph(a_cpu, a_cpu_size, string2, string2, PERCENT, string1 , AREA, "Percent");
		}
        }
	if (lpar_found) {
		fprintf(wfp, "<H3>LPAR</H3>\n");

		if ( (n = findfirst("BBBL,")) != -1) {
			fprintf(wfp, "<UL>\n");
			for (j = 0; j < 18; j++) {
				i = names2array(line[n+j]);
				fprintf(wfp, "<LI>%18s = %s\n", array[0], array[1]);
			}
			fprintf(wfp, "</UL>\n");
		}

		strcpy(array[0], "PhysicalCPU");
		strcpy(array[1], "entitled");
		rrdgraph(array, 2, "lpar", "lpar1", AUTO, "Physical CPU vs Entitlement - LPAR", LINE, "CPU Amount");

		strcpy(array[0], "PhysicalCPU");
		strcpy(array[1], "PoolIdle");
		rrdgraph(array, 2, "lpar", "lpar2", AUTO, "Shared Pool Utilisation(see below for Pool Size) - LPAR", AREA, "CPUs");

		strcpy(array[0], "PhysicalCPU");
		strcpy(array[1], "virtualCPUs");
		strcpy(array[2], "logicalCPUs");
		strcpy(array[3], "poolCPUs");
		strcpy(array[4], "entitled");
		strcpy(array[5] , "PoolIdle");
		strcpy(array[6], "usedAllCPU");
		strcpy(array[7], "usedPoolCPU");
		strcpy(array[8], "SharedCPU");
		rrdgraph(array, 9, "lpar", "lpar3", AUTO, "ALL CPU Stats - LPAR", LINE, "CPUs");

		strcpy(array[0], "weight");
		rrdgraph(array, 1, "lpar", "lpar4", AUTO, "LPAR Weight - LPR", LINE, "Weight Factor");
		fprintf(wfp, "<br>\n");
	}

	if (top_found && no_of_nmon_files == 1) {
		fprintf(wfp, "<H3>TOP Processes</H3> Note: 1 CPU = 100%%, 2 CPU = 200%%, etc.\n");

		/* generate web page top table with this */
		fprintf(wfp, "<table border=1><tr><th>Process Name<th>CPU Percent</tr>\n");
		for (i = 0; i < TOP_PROCESSES / 4; i++) {
			if ( topname[i].cpu < 0)
				break;
			fprintf(wfp, "<tr>\n");
			for (j = 0; j < 4; j++) {
				if ( topname[i+j*5].cpu < 0)
					break;
				fprintf(wfp, "<td>%s<td>%7.2f\n", topname[i+j*5].name, topname[i+j*5].cpu / snapshots);
			}
		}
		fprintf(wfp, "</table>\n");

		for (i = 0; i < TOP_PROCESSES; i++) {
			if ( topname[i].cpu < 0) {
				sprintf(array[i], "none%d", i);
			} else
				strcpy(array[i], topname[i].name);
		}
		rrdcreate(array, TOP_PROCESSES, "top");
		rrdgraph(array, TOP_PROCESSES, "top", "tops", AUTO, "Top Processes", AREA, "Percent of one CPU");
		rrdgraph(array, TOP_PROCESSES, "top", "top", AUTO,  "Top Processes", LINE, "Percent of one CPU");

		fprintf(wfp, "<br>\n");
	}
	if (wlm_found) {
		fprintf(wfp, "<br>\n");
		fprintf(wfp, "<H3>Workload Manager (WLM)</H3>\n");
		rrdgraph(a_wlm, a_wlm_size, "wlmcpu", "wlmcpu", PERCENT, "Workload Manager CPU", AREA, "percentages");
		rrdgraph(a_wlm, a_wlm_size, "wlmmem", "wlmmem", PERCENT, "Workload Manager Memory", AREA, "percentages");
		rrdgraph(a_wlm, a_wlm_size, "wlmbio", "wlmbio", PERCENT, "Workload Manager Block IO", AREA, "percentages");
	}
	fprintf(wfp, "<H3>Memory</H3>\n");
	//do only if not linux
       if(linuxone==0){
	strcpy(array[0], "RealFree");
	rrdgraph(array, 1, "mem", "mem1", PERCENT, "Memory Free Percent - MEM", LINE, "Percent");
	strcpy(array[0], "Virtualfree");
	rrdgraph(array, 1, "mem", "mem2", PERCENT, "Virtual Memory Free Percent - MEM", LINE, "Percent");
       strcpy(array[0], "RealfreeMB");
	rrdgraph(array, 1, "mem", "mem3", AUTO, "Memory Free Size - MEM", LINE, "MBytes");
	strcpy(array[0], "VirtualfreeMB");
	rrdgraph(array, 1, "mem", "mem4", AUTO, "Virtual Memory Free Size - MEM", LINE, "MBytes");

	strcpy(array[0], "RealtotalMB");
	rrdgraph(array, 1, "mem", "mem5", AUTO, "Whole Memory Size - MEM", LINE, "MBytes");
	strcpy(array[0], "VirtualtotalMB");
	rrdgraph(array, 1, "mem", "mem6", AUTO, "Whole Virtual Memory Size - MEM", LINE, "MBytes");

	rrdgraph(a_memnew,memnew_size,"memnew", "memnew", PERCENT,  "Memory Usage - MEMNEW", AREA,"Percent");

	strcpy(array[0], "maxperm");
	strcpy(array[1], "minperm");
	strcpy(array[2], "numperm");
	rrdgraph(array, 3, "memuse", "mem9", PERCENT, "Memory Filesystem Cache - MEMUSE", LINE, "Percent");

	strcpy(array[0], "minfree");
	strcpy(array[1], "maxfree");
	rrdgraph(array, 2, "memuse", "mem10", AUTO, "Memory Freelist - MEMUSE", LINE, "Blocks");

	if (memuse_size >= 7) {
		strcpy(array[0], "numclient");
		strcpy(array[1], "maxclient");
		rrdgraph(array, 2, "memuse", "mem11", PERCENT, "JFS2 Client Memory - MEMUSE", LINE, "Percent");
	}
        }else{
			
	/*Start Linux */

        rrdgraph(a_mem15,a_mem_size15, "mem","memlinux1",AUTO, "Total Memory",LINE,"Percent");
/*strcpy(array[0],"hightotal");
rrdgraph(array,1, "mem","memlinux2",PERCENT, "High Total Memory",LINE,"Percent");
strcpy(array[0],"lowtotal");
rrdgraph(array,1, "mem","memlinux3",PERCENT, "Low Total Memory",LINE,"Percent");*/

/*End Linux */}

	if (ame_found) {
		fprintf(wfp, "<br><H3>Active Memory Expansion</H3>\n");

		strcpy(array[0], "CompressedPool");
		rrdgraph(array, 1, "memnew", "ame1", AUTO, "Compresses Pool Percent", LINE, "percent");

		strcpy(array[0], "AME_pgins");
		strcpy(array[1], "AME_pgouts");
		rrdgraph(array, 2, "page", "ame2", AUTO, "Compresses Pool paging", LINE, "pages per second");

		/*Size of the Compressed pool(MB), Size of true memory(MB), Expanded memory size(MB), Size of the Uncompressed pool(MB)*/
		strcpy(array[0], "AME_Compressed");
		strcpy(array[1], "AME_True");
		strcpy(array[2], "AME_Expanded");
		strcpy(array[3], "AME_Uncompressed");
		rrdgraph(array, 4, "mem", "ame3", AUTO, "Compresses Pool Sizes", LINE, "MBytes");
	}

	fprintf(wfp, "<br>\n");
if(linuxone==0){
	rrdcreate(a_page,a_page_size,"vm");
	rrdgraph(a_page,a_page_size,"vm","page1",AUTO,"plaaa",LINE,"vm");

	strcpy(array[0], "faults");
	strcpy(array[1], "reclaims");
	strcpy(array[2], "scans");
	strcpy(array[3], "cycles");
	rrdgraph(array, 4, "page", "page1", AUTO, "Paging VMM Stats - PAGE", LINE, "Operations per second");

	strcpy(array[0], "pgsin");
	strcpy(array[1], "pgsout");
	rrdgraph(array, 2, "page", "page2", AUTO, "Pagespace Paging - PAGE", LINE, "Pages per second");

	strcpy(array[0], "pgin");
	strcpy(array[1], "pgout");
	rrdgraph(array, 2, "page", "page3", AUTO, "Filesystem Paging - PAGE", LINE, "Pages per second");}

	/* PAGING */
	if (paging_found) {
		fprintf(wfp, "<br>\n");
		fprintf(wfp, "<H3>Paging Space</H3>\n");
		rrdgraph(a_paging, a_paging_size, "paging", "paging", AUTO, "Paging Space Free in MBs - PAGING", LINE, "MBs");
	}

	if (largepage_found) {
		fprintf(wfp, "<br>\n");
		fprintf(wfp, "<H3>Large Page</H3>\n");
		rrdgraph(a_largepage, a_largepage_size, "largepage", "largepage", AUTO, "Large Page Stats", LINE, "Pages and Siaz in MBs");
	}
	fprintf(wfp, "<br>\n");
	fprintf(wfp, "<H3>Process Stats</H3>\n");

	strcpy(array[0], "Runnable");
	rrdgraph(array, 1, "proc", "procrunq", AUTO, "Run Queue - PROC", LINE, "Process on Queue");
if(linuxone == 0){
	strcpy(array[0], "Swapin");
	rrdgraph(array, 1, "proc", "swapin", AUTO, "Swapin - PROC", LINE, "Operations per second");}
	strcpy(array[0], "pswitch");
	rrdgraph(array, 1, "proc", "pswitch", AUTO, "Process Switch - PROC", LINE, "Per Second");
	strcpy(array[0], "syscall");
	rrdgraph(array, 1, "proc", "syscall", AUTO, "System Calls - PROC", LINE, "Per Second");
	strcpy(array[0], "read");
	strcpy(array[1], "write");
	rrdgraph(array, 2, "proc", "readwrite", AUTO, "System Calls(read/write) - PROC", LINE, "Per Second");
	strcpy(array[0], "fork");
	strcpy(array[1], "exec");
	rrdgraph(array, 2, "proc", "forkexec", AUTO, "System Calls(fork/exec) - PROC", LINE, "Per Second");
	strcpy(array[0], "sem");
	strcpy(array[1], "msg");
	rrdgraph(array, 2, "proc", "ipc", AUTO, "System Calls(sem/msg) - PROC", LINE, "Per Second");

	if (aio_found == 1) {
		fprintf(wfp, "<br>\n");
		fprintf(wfp, "<H3>Asynchronous IO Servers</H3>\n");
		rrdgraph(a_aio, a_aio_size, "procaio", "procaio", AUTO, "AIO Stats", LINE, "Various");
	}
	fprintf(wfp, "<br>\n");
	fprintf(wfp, "<H3>Filesystem</H3>\n");
	strcpy(array[0], "iget");
	strcpy(array[1], "namei");
	strcpy(array[2], "dirblk");
	rrdgraph(array, 3, "file", "file", AUTO, "File System functions - FILE", LINE, "Per Second");
	strcpy(array[0], "readch");
	strcpy(array[1], "writech");
	rrdgraph(array, 2, "file", "filerw", AUTO, "File System read/write - FILE", LINE, "Per Second");

	fprintf(wfp, "<br>\n");
	fprintf(wfp, "<H3>Filesystem Use</H3>\n");
	fprintf(wfp, "<table border=1><tr><th>Filesystem Number</th><th>Mount Point</th></tr>\n");
	for (i = 0; i < a_jfs_size; i++) {
		fprintf(wfp, "<tr><td>%s</td><td>%s</td></tr>\n", a_jfsdummy[i], a_jfs[i]);
	}
	fprintf(wfp, "</table>\n");

	rrdgraph(a_jfsdummy, a_jfs_size, "jfsfile",  "jfsfile",  PERCENT, "JFS Percent Full", LINE, "Percent");
	rrdgraph(a_jfsdummy, a_jfs_size, "jfsinode", "jfsinode", PERCENT, "JFS Inode Percent Full", LINE, "Percent");

	fprintf(wfp, "<br>\n");
	fprintf(wfp, "<H3>Network</H3>\n");
	rrdgraph(a_net, a_net_size, "net", "nettotal", AUTO, "Network Throughput - NET", AREA, "Kbytes per second");
	rrdgraph(a_net, a_net_size, "net", "net", AUTO, "Network Throughput - NET", LINE, "Kbytes per second");

	if (neterror_found)
		rrdgraph(a_neterr, a_neterr_size, "neterror", "neterror", AUTO, "Network Errors", LINE, "Errors/Collisions per second");

	if (netpacket_found == 1)
		rrdgraph(a_net, a_net_size, "netpacket", "netpacket", AUTO, "Network - NET", LINE, "packet size");

	if ( nfscliv2_found || nfscliv3_found || nfscliv4_found || nfssvrv2_found || nfssvrv3_found || nfssvrv4_found) {
		fprintf(wfp, "<br>\n");
		fprintf(wfp, "<H3>Network File System (NFS)</H3>\n");
		if(nfssvrv2_found)
			rrdgraph(a_nfssvrv2, a_nfssvrv2_size, "nfssvrv2", "nfssvrv2", AUTO, "NFS Server v2", LINE, "Calls per second");
		if(nfscliv2_found)
			rrdgraph(a_nfscliv2, a_nfscliv2_size, "nfscliv2", "nfscliv2", AUTO, "NFS Client v2", LINE, "Calls per second");
		if(nfssvrv3_found)
			rrdgraph(a_nfssvrv3, a_nfssvrv3_size, "nfssvrv3", "nfssvrv3", AUTO, "NFS Server v3", LINE, "Calls per second");
		if(nfscliv3_found)
			rrdgraph(a_nfscliv3, a_nfscliv3_size, "nfscliv3", "nfscliv3", AUTO, "NFS Client v3", LINE, "Calls per second");
		if(nfssvrv4_found)
			rrdgraph(a_nfssvrv4, a_nfssvrv4_size, "nfssvrv4", "nfssvrv4", AUTO, "NFS Server v4", LINE, "Calls per second");
		if(nfscliv4_found)
			rrdgraph(a_nfscliv4, a_nfscliv4_size, "nfscliv4", "nfscliv4", AUTO, "NFS Client v4", LINE, "Calls per second");
	}
	if (fc_found) {
		fprintf(wfp, "<br>\n");
		fprintf(wfp, "<H3>Fibre Channel</H3>\n");
		rrdgraph(a_fc, a_fc_size, "fcread",  "fcread", AUTO, "FC Adapter Read - FCSTAT",  LINE, "Kbytes per second");
		rrdgraph(a_fc, a_fc_size, "fcwrite", "fcwrite", AUTO, "FC Adapter Write - FCSTAT", LINE, "Kbytes per second");
		rrdgraph(a_fc, a_fc_size, "fcxferin",  "fcxferin", AUTO, "FC Adapter Transfers In - FCSTAT",  LINE, "packets per second");
		rrdgraph(a_fc, a_fc_size, "fcxferout", "fcxferout", AUTO, "FC Adapter Transfers Out - FCSTAT", LINE, "Packets per second");
	}
	if (adapt_found) {
		fprintf(wfp, "<br>\n");
		fprintf(wfp, "<H3>Disk Adapter</H3>\n");
		rrdgraph(a_ioa, a_ioa_size, "ioadapt", "iototal", AUTO, "Disk Adapter - IOADAPT", AREA, "Kbytes per second");
		rrdgraph(a_ioa, a_ioa_size, "ioadapt", "ioadapt", AUTO, "Disk Adapter - IOADAPT", LINE, "Kbytes per second");
	}

	fprintf(wfp, "<br>\n");
	if (dg_found) {
		fprintf(wfp, "<H3>Disk Group</H3>\n");
		rrdgraph(a_dg, a_dg_size, "dgbusy", "dgbusy",     PERCENT, "Disk Group Busy", LINE, "Percent");
		rrdgraph(a_dg, a_dg_size, "dgsize", "dgsize",     AUTO, "Disk Group Block Size", LINE, "KBytes");
		rrdgraph(a_dg, a_dg_size, "dgread", "dgreadtotal", AUTO, "Disk Group Read", AREA, "KBytes per second");
		rrdgraph(a_dg, a_dg_size, "dgread", "dgread",     AUTO, "Disk Group Read", LINE, "KBytes per second");
		rrdgraph(a_dg, a_dg_size, "dgwrite", "dgwritetotal", AUTO, "Disk Group Write", AREA, "KBytes per second");
		rrdgraph(a_dg, a_dg_size, "dgwrite", "dgwrite",   AUTO, "Disk Group Disk Group", LINE, "KBytes per second");
		rrdgraph(a_dg, a_dg_size, "dgxfer", "dgxfertotal", AUTO, "Disk Group Transfers", AREA, "Transfers per second");
		rrdgraph(a_dg, a_dg_size, "dgxfer", "dgxfer",     AUTO, "Disk Group Transfers", LINE, "Transfers per second");
		fprintf(wfp, "<br>\n");
	}
	if (ess_found) {
		fprintf(wfp, "<H3>ESS</H3>\n");
		rrdgraph(a_ess, a_ess_size, "essread", "essreadtotal",    AUTO, "ESS Read", AREA, "KBytes per second");
		rrdgraph(a_ess, a_ess_size, "essread", "essread",         AUTO, "ESS Read", LINE, "KBytes per second");
		rrdgraph(a_ess, a_ess_size, "esswrite", "esswritetotal",  AUTO, "ESS Write", AREA, "KBytes per second");
		rrdgraph(a_ess, a_ess_size, "esswrite", "esswrite",       AUTO, "ESS Write", LINE, "KBytes per second");
		rrdgraph(a_ess, a_ess_size, "essxfer", "essxfertotal",    AUTO, "ESS Transfers", AREA, "Transfers per second");
		rrdgraph(a_ess, a_ess_size, "essxfer", "essxfer",         AUTO, "ESS Transfers", LINE, "Transfers per second");
		fprintf(wfp, "<br>\n");
	}
	fprintf(wfp, "<H3>Disks</H3>\n");
	for (j = 0; j < disksects; j++) {
		fprintf(wfp, "<H4>Disks Set %d</H4>\n", j);
		sprintf(string2, j ? "diskbusy%d" : "diskbusy", j);
		rrdgraph(a_disk[j], a_disk_size[j], string2, string2, PERCENT, "DISK Busy", LINE, "Percent");
		sprintf(string2, j ? "diskbsize%d" : "diskbsize", j);
		rrdgraph(a_disk[j], a_disk_size[j], string2, string2, AUTO, "DISK Block Size", LINE, "KBytes");

		sprintf(string1, j ? "diskread%d" : "diskread", j);
		sprintf(string2, j ? "diskread%dtotal" : "diskreadtotal", j);
		rrdgraph(a_disk[j], a_disk_size[j], string1, string2, AUTO, "DISK Read", AREA, "KBytes per second");
		rrdgraph(a_disk[j], a_disk_size[j], string1, string1, AUTO, "DISK Read", LINE, "KBytes per second");

		sprintf(string1, j ? "diskwrite%d" : "diskwrite", j);
		sprintf(string2, j ? "diskwrite%dtotal" : "diskwritetotal", j);
		rrdgraph(a_disk[j], a_disk_size[j], string1, string2, AUTO, "DISK Write", AREA, "KBytes per second");
		rrdgraph(a_disk[j], a_disk_size[j], string1, string1, AUTO, "DISK Write", LINE, "KBytes per second");

		sprintf(string1, j ? "diskxfer%d" : "diskxfer", j);
		sprintf(string2, j ? "diskxfer%dtotal" : "diskxfertotal", j);
		rrdgraph(a_disk[j], a_disk_size[j], string1, string2, AUTO, "DISK Transfers", AREA, "Transfers per second");
		rrdgraph(a_disk[j], a_disk_size[j], string1, string1, AUTO, "DISK Transfers", LINE, "Transfers per second");
	}

	/* webtail */
	fprintf(wfp, "</BODY></HTML>\n");

	sprintf(filename, "%s/%s", dirname, "rrd_update");
	if ( (ufp = fopen(filename, "w")) == NULL) {
		perror("failed to open file");
		printf("file: \"%s\"\n", filename);
		exit(75);
	}
     
	/* this is the loop that creates the bulk of the rrd update file */
	for (j = 0; j < lines; j++) {
		if ( !strncmp( line[j], "FRCA", 4) )
			continue;
		if ( !strncmp( line[j], "DISKRSIZE", 9) )
			continue;
		if ( !strncmp( line[j], "DISKWSIZE", 9) )
			continue;
		if ( !strncmp( line[j], "UARG", 4) )
			continue;
		if ( !strncmp( line[j], "TOP", 3) )
			continue;
		if ( !strncmp( line[j], "BBB", 3) )
			continue;
		if ( !strncmp( line[j], "ZZZZ", 4) )
			continue;
		//if ( !strncmp( line[j], "CPU00", 5) )
		//	continue;
		if ( !strncmp( line[j], "CPU000", 6) )
			continue;
		if ( nmon9 && !strncmp( line[j], "IOADAPT", 7) )
			continue;
		if ( !strncmp( line[j], "CPU_EC", 6) )
			continue;
		if ( !strncmp( line[j], "CPU_VP", 6) )
			continue;
		if ( !strncmp( line[j], "VGBUSY", 6) )
			continue;
		if ( !strncmp( line[j], "VGREAD", 6) )
			continue;
		if ( !strncmp( line[j], "VGWRITE", 7) )
			continue;
		if ( !strncmp( line[j], "VGSIZE", 6) )
			continue;
		if ( !strncmp( line[j], "VGXFER", 6) )
			continue;
		if ( !strncmp( line[j], "CPU01", 5) && cpus == 1)
			continue;
		if ( !strncmp( line[j], "PCPU", 4) )
			continue;
		if ( !strncmp( line[j], "SCPU", 4) )
			continue;
		if ( !strncmp( line[j], "POOLS", 5) )
			continue;
		if ( !strncmp( line[j], "DISKRXFER", 9) )
			continue;
		if ( !strncmp( line[j], "DISKRIO", 7) )
			continue;
		if ( !strncmp( line[j], "DISKWIO", 7) )
			continue;
		if ( !strncmp( line[j], "DISKAVGRIO", 10) )
			continue;
		if ( !strncmp( line[j], "DISKAVGWIO", 10) )
			continue;
		if ( !strncmp( line[j], "NETSIZE", 7) )
			continue;
		if ( !strncmp( line[j], "ERROR", 5) ) /* These are reall error lines */
			continue;
		if ( !strncmp( line[j], "WLMCPU", 6) )
			continue;
		if ( !strncmp( line[j], "WLMMEM", 6) )
			continue;
		if ( !strncmp( line[j], "WLMBIO", 6) )
			continue;
		if ( !strncmp( line[j], "MEMPAGES4KB", 11) )
			continue;
		if ( !strncmp( line[j], "MEMPAGES64KB", 12) )
			continue;
		if ( !strncmp( line[j], "MEMPAGES16MB", 12) )
			continue;
		if ( !strncmp( line[j], "MEMPAGES16GB", 12) )
			continue;
		if ( !strncmp( line[j], "WPARCPU", 7) )
			continue;
		if ( !strncmp( line[j], "WPARMEM", 7) )
			continue;
		if ( !strncmp( line[j], "RAWLPAR", 7) )
			continue;
		if ( !strncmp( line[j], "RAWCPUTOTAL", 11) )
			continue;
		if ( !strncmp( line[j], "DISKWAIT", 8) )
			continue;
		if ( !strncmp( line[j], "DISKSERV", 8) )
			continue;
		if ( !strncmp( line[j], "DISKREADSERV", 12) )
			continue;
		if ( !strncmp( line[j], "DISKWRITESERV", 13) )
			continue;
		if ( !strncmp( line[j], "DONATE", 6) )
			continue;
		if ( !strncmp( line[j], "SEA", 3) )
			continue;


		/* fprintf(stderr,"line is %s\n", line[j]); */

		if ( (i = search_for_tstring(line[j])) != 0) {
			sscanf(&line[j][i+1], "%d", &tnum);
			if ( string[strlen(line[j])-1] == '\n') {
				string[strlen(line[j])-1] = 0;
			}
			if ( string[strlen(line[j])-1] == ',') {
				string[strlen(line[j])-1] = 0;
			}
			for (s = line[j]; *s != 0; s++) {
				if ( *s == ',' && *(s + 1) == ',') {
					*(s) = 0; /* truncate at missing data - helps CPU_ALL */
				}
				if (*s == ',')
					*s = ':';
				/* what if int data has spaces !? ..10, 101,1    MARK101*/
				if (*s == ' ')
					*s = '0';
			}
			line[j][i-1] = 0; /* -1 so we hit the , */
			for (s = line[j]; *s != 0; s++) {
				*s = tolower(*s);
			}
			fprintf(ufp, "update %s.rrd %ld:%s\n", line[j], tarray[tnum], &line[j][i+6]);
		}
	}
	file_io_end();
	if (execute) {
		chdir(dirname);
		run("rm -f *.rrd");
		run("rm -f *.gif");
		run("rrdtool - < rrd_create");
		run("rrdtool - < rrd_update >rrd_update.log");
		if (top_found)
			run("rrdtool - < rrd_top >rrd_top.log");
		run("rrdtool - < rrd_graph");
	} else {
		printf("Manually complete with:\n");
		printf("cd %s\n", dirname);
		printf("rm -f *.rrd\n");
		printf("rm -f *.gif\n");
		printf("rrdtool - < rrd_create\n");
		printf("rrdtool - < rrd_update >rrd_update.log\n");
		if (top_found)
			printf("rrdtool - < rrd_top >rrd_top.log\n");
		printf("rrdtool - < rrd_graph\n");
	}
	return 0;
}


