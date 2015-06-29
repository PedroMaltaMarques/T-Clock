//Based on autorevision.cpp by Code::Blocks released as GPLv3 (should be enough modification to vanish GPL, MUHAHAHAHAW)
//	Rev: 7109 @ 2011-04-15T11:53:16.901970Z
//Now released as WTFPL
#include <stdio.h>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <string>
using namespace std;

#define AV_VERSION "1.0.1"

#ifdef _MSC_VER
#	include <direct.h>//unlink,getcwd
#	define PATH_MAX 260
#	define popen _popen
#	define pclose _pclose
#	define unlink _unlink
#	define getcwd _getcwd
#	define sscanf sscanf_s
#	define chdir _chdir
#	define stricmp _stricmp
#	define fopen fopenMShit
	FILE* fopenMShit(const char* filename, const char* mode){
		FILE* ret; fopen_s(&ret,filename,mode);
	return ret;
	}
#	define gmtime gmtimeMShit
	struct tm* gmtimeMShit(const time_t* time){
		static struct tm ret; gmtime_s(&ret,time);
		return &ret;
	}
#	define getenv getenvMShit
	char* getenvMShit(const char* varname){
		size_t wayne;
		static char ret[2048];
		getenv_s(&wayne,ret,sizeof(ret),varname);
		return ret;
	}
#else
#	include <unistd.h>//unlink,getcwd
#	include <stdlib.h>//getenv,setenv
#	ifdef __linux__
#		include <linux/limits.h>//PATH_MAX
#		define stricmp strcasecmp
#	endif // __linux__
#endif

#ifdef _WIN32
#	include <windows.h>//ExpandEnvironmentStrings
#	define setenv(name,value,force) SetEnvironmentVariable(name,value)
#endif

enum{
	REPO_NONE     =0x00,
	REPO_AUTOINC  =0x01, // fake repo, simple autoincrement
	REPO_GIT      =0x02,
	REPO_SVN      =0x04,
};
enum{
	FLAG_NONE     =0x00,
	FLAG_PRE      =0x01,
	FLAG_POST     =0x02,
	FLAG_E_IF     =0x04,
	FLAG_E_IFNOT  =0x08,
	FLAG_VERBOSE  =0x10,
	FLAG_ERROR    =0x80,
};
unsigned char g_flag = FLAG_PRE;
unsigned char g_repo = REPO_AUTOINC;

//major.minor[.build[.revision]]
//0.1a1
//1.0r1
//1.0.1 #2141
//    1.2.0.1 instead of 1.2-a1
//    1.2.1.2 instead of 1.2-b2 (beta with some bug fixes)
//    1.2.2.3 instead of 1.2-rc3 (release candidate)
//    1.2.3.0 instead of 1.2-r (commercial distribution)
//    1.2.3.5 instead of 1.2-r5 (commercial distribution with many bug fixes)
enum Status{
	STATUS_Alpha = 0,
	STATUS_Beta,
	STATUS_ReleaseCandidate,
	STATUS_Release,
	STATUS_ReleaseMaintenance,
	STATUS_NUM_};
//const char* STATUS_S[STATUS_NUM_]   = {"Alpha","Beta","Release Candidate","Release","Release Maintenance"};
const char* STATUS_S[STATUS_NUM_]   = {"Alpha","Beta","RC","Release","Maintenance"};
const char* STATUS_SS[STATUS_NUM_]  = {"a","b","rc","r","rm"};
const char* STATUS_SS2[STATUS_NUM_] = {"α","β","гc","г","гm"};
enum VersionFlags{
	VER_DIRTY = 0x01,
};
struct Version{
	unsigned char flags_;
	unsigned char major;
	unsigned short minor;
	unsigned short build;
	unsigned char status;
	unsigned int revision;
	time_t timestamp;
	string url;
	string date;
	string revhash;
};

bool ReadHeader(const char* filepath,Version &ver);
bool QueryGit(const char* path,Version* ver);
bool QuerySVN(const char* path,Version* ver);
void PrintDefine(FILE* fp,const char* define,const Version &ver);
bool WriteHeader(const char* filepath,Version &ver);

void SetupPath(const char* paths) {
#	ifdef _WIN32
#		define PATH_DELIM ";"
#	else
#		define PATH_DELIM ":"
#	endif // _WIN32
	string path;
	const char* envp;
	size_t len = 0;
	if((envp=getenv("PATH")))
		path += envp;
	if(paths && paths[0]){
		path += string(PATH_DELIM) + paths;
		++len;
	}
	if((envp=getenv("AUVER_PATH")) && envp[0]){
		path += string(PATH_DELIM) + envp;
		++len;
	}
#	ifdef _WIN32
	if(!len)
		path += ";%ProgramW6432%/Git/bin;%ProgramFiles(x86)%/Git/bin;%LocalAppData%/GitHub/PORTAB~1/bin;%ProgramW6432%/TortoiseSVN/bin;%ProgramFiles(x86)%/TortoiseSVN/bin";
	len = ExpandEnvironmentStrings(path.c_str(), NULL, 0);
	char* new_path = (char*)malloc(len);
	if(!new_path)
		return;
	ExpandEnvironmentStrings(path.c_str(), new_path, len);
	setenv("PATH", new_path, 1);
	//puts(new_path);
	free(new_path);
#	else
	if(len)
		setenv("PATH", path.c_str(), 1);
	//puts(path.c_str());
#	endif // _WIN32
}

#ifndef _MSC_VER
#	include <getopt.h>
#else // _MSC_VER
#	define no_argument         no_argument_msvc
#	define required_argument   required_argument_msvc
#	define optional_argument   optional_argument_msvc
#	define option              option_msvc
#	define optind              optind_msvc
#	define optopt              optopt_msvc
#	define opterr              opterr_msvc
#	define optarg              optarg_msvc
#	define getopt_long         getopt_long_msvc
struct option_msvc{
	const char* name;
	int has_arg;
	int* flag;
	int val;
};
int optind_msvc = 1; /* index of first non-option in argv      */
int optopt_msvc = 0; /* single option character, as parsed     */
int opterr_msvc = 1; /* flag to enable built-in diagnostics... */
                     /* (user may set to zero, to suppress)    */
char* optarg_msvc = NULL; /* pointer to argument of current option  */
enum HAS_ARG{
	no_argument_msvc = 0,   /* option never takes an argument */
	required_argument_msvc, /* option always requires an argument */
	optional_argument_msvc  /* option may take an argument */
};
// basic implementation, doesn't support GNU extensions in `optstring` (+-) or POSIXLY_CORRECT
// http://linux.die.net/man/3/getopt_long
int getopt_long_msvc(int argc, char*const argv[], const char* optstring, const struct option* longopts, int* longindex){
	static int s_idx = 1;
	static const char* s_nextchar = NULL;
	int idx;
	const char* opt;
	int len;
	#define printOptErr(fmt,...) if(opterr && *optstring!=':') fprintf(stderr, "%s: " fmt, argv[0], ##__VA_ARGS__)
	if(optind <= 1){
		optind = 2;
		s_idx = 1;
		s_nextchar = NULL;
	}
	if(longindex)
		*longindex = 0;
	optarg = NULL;
	for(;;){
	if(s_idx >= argc)
		return -1;
	if(!s_nextchar){ // new param
		if(argv[s_idx][0] != '-' || !argv[s_idx][1]){
			// not a parameter, search for next parameter
			for(idx=optind; idx<argc; ++idx){
				if(argv[idx][0] == '-' && argv[idx][1]){
					char** nargv = (char**)argv;
					int oldidx = s_idx;
					int move;
					int result;
					// parse param
					optind = idx;
					s_idx = optind++;
					s_nextchar = NULL;
					result = getopt_long(argc, argv, optstring, longopts, longindex);
					// permutate argv
					if(result == -1) // --
						s_idx = idx+1;
					len = s_idx-idx;
					optind = oldidx + len;
					s_idx = optind++;
					do{
						opt = nargv[idx];
						for(move=idx; move-- > oldidx; ){
							nargv[move+1] = nargv[move];
						}
						nargv[oldidx] = (char*)opt;
						++idx; ++oldidx;
					}while(len-- > 1);
					if(result == -1)
						break;
					return result;
				}
			}
			optind = s_idx;
			s_idx = argc;
			return -1;
		}
		s_nextchar = argv[s_idx]+1;
		if(*s_nextchar == '-'){
			// long option
			if(!*++s_nextchar){
				s_idx = argc;
				return -1;
			}
			opt = s_nextchar;
			s_idx = optind++;
			s_nextchar = strchr(opt, '=');
			if(!s_nextchar)
				s_nextchar = strchr(opt, '\0');
			len = s_nextchar - opt;
			s_nextchar = NULL;
			for(idx=0; longopts[idx].name; ++idx){
				if(!strncmp(opt, longopts[idx].name, len)){
					optopt = longopts[idx].val;
					if(longindex)
						*longindex = idx;
					if(longopts[idx].flag){
						*longopts[idx].flag = longopts[idx].val;
						return 0;
					}
					if(longopts[idx].has_arg != no_argument){
						if(opt[len] == '='){
							optarg = (char*)opt+len+1;
						}else if(s_idx < argc){
							optarg = argv[s_idx];
							s_idx = optind++;
						}else if(longopts[idx].has_arg != optional_argument){
							if(opterr){
								printOptErr("option requires an argument -- %s\n", opt);
								return (*optstring==':'?':':'?');
							}
							optarg = (char*)"-";
						}
					}
					return longopts[idx].val;
				}
			}
			optopt = 0;
			printOptErr("unknown option -- %.*s\n", len, opt);
			return '?';
			// end long option
		}
	}
	// short option
	if(!*s_nextchar){
		s_idx = optind++;
		s_nextchar = NULL;
		continue;
	}
	for(opt=optstring; *opt; ++opt){
		if(*opt == ':' || *opt == '+' || *opt == '-')
			continue;
		if(*opt == *s_nextchar)
			break;
	}
	optopt = *s_nextchar++;
	if(*opt){
		if(opt[1] == ':'){
			optarg = (char*)s_nextchar;
			s_idx = optind++;
			s_nextchar = NULL;
			if(!*optarg){
				optarg = NULL;
				if(s_idx < argc){
					optarg = argv[s_idx];
					s_idx = optind++;
				}else if(opt[2] != ':'){
					printOptErr("option requires an argument -- %c\n", (char)optopt);
					return (*optstring==':'?':':'?');
				}
			}
		}
		return *opt;
	}
	printOptErr("unknown option -- %c\n", (char)optopt);
	return '?';
	// end short option
	}
}
#endif

#define DH_ARGV 0
#define DH_ARGV_SHORT (const char*)1
struct help{
	int opt;
	const char* params;
	const char* descr;
};
const char* PrintIndentedLine(const char* str, int max_line/**< 80 */, int indented, int indent){
	const char* eol;
	max_line -= indent+2;
	if(!*str){
		putc('\n', stdout);
		return str;
	}
	for(; indented<indent; ++indented) putc(' ', stdout);
	eol = strchr(str, '\n');
	if(!eol)
		eol = strchr(str, '\0');
	if(eol-str > max_line){
		eol = str + max_line;
		for(; *eol > ' ' && eol != str; --eol);
		if(eol == str)
			eol = str + max_line - 1;
	}
	printf("  %.*s\n", eol-str, str);
	if(*eol <= ' ' && *eol)
		return eol + 1;
	return eol;
}
int DisplayHelp(const char* argv0, const char* short_options, const struct option* long_options, const struct help* help_info){
	size_t maxlen = 0;
	size_t len;
	int measure;
	int idx;
	int opt;
	const char* offset;
	// usage
	if(help_info[0].params){
		if(help_info[0].params == DH_ARGV_SHORT){
			const char* short_name;
			short_name = strrchr(argv0, '/');
			if(!short_name)
				short_name = strrchr(argv0, '\\');
			if(!short_name)
				short_name = argv0-1;
			argv0 = short_name+1;
		} else
			argv0 = help_info[0].params;
	}
	printf("Usage:   %s %s\n", argv0, help_info[0].descr);
	// get indent part one
	for(opt=0; long_options[opt].name; ++opt){
		len = 6 + strlen(long_options[opt].name);
		if(len > maxlen)
			maxlen = len;
	}
	// options
	puts("Options:");
	measure = 1;
	do{
		for(idx=1; help_info[idx].descr; ++idx){
			len = 2;
			for(offset=short_options; *offset; ++offset){
				if(help_info[idx].opt == *offset){
					len += 2; // -x
					if(!measure)
						printf("  -%c", help_info[idx].opt);
					break;
				}
			}
			opt = 0;
			if(!*offset){
				for(; long_options[opt].name; ++opt){
					if(long_options[opt].val == help_info[idx].opt){
						len += 2 + strlen(long_options[opt].name); // --x
						if(!measure)
							printf("  --%s", long_options[opt++].name);
						break;
					}
				}
				if(!long_options[opt].name)
					opt = 0;
			}
			if(!len)
				break;
			if(help_info[idx].params){
				len += 1 + strlen(help_info[idx].params);
				if(!measure)
					printf(" %s", help_info[idx].params);
			}
			
			if(measure){
				if(len > maxlen)
					maxlen = len;
				continue;
			}
			
			offset = PrintIndentedLine(help_info[idx].descr, 80, len, maxlen);
			for(; long_options[opt].name; ++opt){
				if(long_options[opt].val == help_info[idx].opt){
					len = printf("    --%s", long_options[opt].name);
					offset = PrintIndentedLine(offset, 80, len, maxlen);
				}
			}
			while(*offset)
				offset = PrintIndentedLine(offset, 80, 0, maxlen);
		}
	}while(measure--);
	return maxlen;
}

int GetEnvBool(const char* var){
	const char* env = getenv(var);
	if(!env)
		return -1;
	// *empty*, 0, 'null', 'n', 'no' nor 'false'
	switch(tolower(env[0])){
	case 0:
		return 0;
	case '0':
	case 'n':
		return !(!env[1] || !stricmp(env,"no") || !stricmp(env,"null"));
	case 'f':
		return stricmp(env,"false");
	}
	return 1;
}

int main(int argc, char** argv)
{
	const char* additional_paths = NULL;
	const char* headerPath;
	char* Gitpath = NULL;
	char* SVNpath = NULL;;
	const char* get_define = NULL;
	static const char* short_options = "hvVa:g:s:d:e:E:pPI";
	static struct option long_options[] = {
		// basic
		{"help",           no_argument,       0, 'h'},
		{"version",        no_argument,       0, '1'},
		{"verbose",        no_argument,       0, 'v'},
		{"brief",          no_argument,       0, 'V'},
		// repo settings
		{"path",           required_argument, 0, 'a'},
		{"git",            required_argument, 0, 'g'},
		{"svn",            required_argument, 0, 's'},
		// misc features
		{"get",            required_argument, 0, 'd'},
		{"if",             required_argument, 0, 'e'},
		{"if-not",         required_argument, 0, 'E'},
		// build features
		{"post-build",     no_argument,       0, 'p'},
		{"post",           no_argument,       0, 'p'},
		{"no-post-build",  no_argument,       0, 'P'},
		{"no-post",        no_argument,       0, 'P'},
//		{"increment",      no_argument,       0, 'i'},
//		{"inc",            no_argument,       0, 'i'},
		{"no-increment",   no_argument,       0, 'I'},
		{"no-inc",         no_argument,       0, 'I'},
		{0}
	};
	const struct help help_info[] = {
		{0,DH_ARGV_SHORT,"[options] [version.h]"},
		{'h',0,"this help message"},
		{'1',0,"displays version"},
		{'v',0,"be verbose"},
		//
		{'a',"<paths>","additional paths to append to PATH variable. Works like the AUVER_PATH environment variable"},
		{'g',"<repository>","use Git repo for 'revision', also add Git date & URL"},
		{'s',"<repository>","use SVN repo for 'revision', also add SVN date & URL"},
		//
		{'d',"<define>","display <define> and exit"},
		{'e',"<ENV>","only update version.h if specified environment variable is true. 'true' means that the content is neither:\n   *empty*, 0, 'null', 'n', 'no' nor 'false'"},
		{'E',"<ENV>","inverse of -e, so don't update if 'true'."},
		//
		{'p',0,"execute post-build stuff now and exit\n(cleans lockfile to re-enable auto increment)\nNote: only needed if no repository was used"},
		{'P',0,"disable post-build\n(don't create lockfile to disable auto increment)"},
//		{'i',0,"-"},
		{'I',0,"do not auto increment revision"},
		{0}
	};
	for(;;){
		int opt;
		int option_index = 0;
		opt = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (opt == -1)
			break;
		switch(opt){
		case 0: case 1:
			break;
		case '?': /*case ':':*/
			g_flag |= FLAG_ERROR;
			break;
		case 'h':
			option_index = DisplayHelp(argv[0], short_options, long_options, help_info);
			printf("Environment variables:\n"
			       "  AUVER_PATH");
			PrintIndentedLine("see --path", 80, 12, option_index);
			printf("  AUVER_IF");
			PrintIndentedLine("see --if", 80, 10, option_index);
			printf("  AUVER_IF_NOT");
			PrintIndentedLine("see --if-not", 80, 14, option_index);
			puts("");
			/* fall through */
		case '1':
			puts("AutoVersion " AV_VERSION);
			return 1;
		case 'v': g_flag |= FLAG_VERBOSE; break;
		case 'V': g_flag &= ~FLAG_VERBOSE; break;
		//
		case 'a': additional_paths = optarg; break;
		case 'g':
			g_repo |= REPO_GIT;
			Gitpath = optarg;
			break;
		case 's':
			g_repo |= REPO_SVN;
			SVNpath = optarg;
			break;
		//
		case 'd': get_define = optarg; break;
		case 'e': // if
			if(GetEnvBool(optarg) != 1){
				printf("if: '%s' is false\n", optarg);
				g_flag |= FLAG_E_IF;
			}
			break;
		case 'E': // if-not
			if(GetEnvBool(optarg) == 1){
				printf("if-not: '%s' is true\n", optarg);
				g_flag |= FLAG_E_IFNOT;
			}
			break;
		//
		case 'p': g_flag |= FLAG_POST; break;
		case 'P': g_flag &= ~FLAG_PRE; break;
		case 'I': g_repo &= ~REPO_AUTOINC; break;
		default:
			;
		}
	}
	if(GetEnvBool("AUVER_IF") == 0){
		printf("AUVER_IF = %s\n", getenv("AUVER_IF"));
		g_flag |= FLAG_E_IF;
	}
	if(GetEnvBool("AUVER_IF_NOT") == 1){
		printf("AUVER_IF_NOT = %s\n", getenv("AUVER_IF_NOT"));
		g_flag |= FLAG_E_IFNOT;
	}
	if(g_flag & (FLAG_E_IF|FLAG_E_IFNOT)){
		puts("	version unchanged");
		return 0;
	}
	if(g_flag & FLAG_ERROR)
		return 2;
	if(g_repo&(REPO_GIT|REPO_SVN) && (!Gitpath&&!SVNpath)) {
		DisplayHelp(argv[0], short_options, long_options, help_info);
		return 1;
	}
	
	SetupPath(additional_paths);
	
	if(optind < argc)
		headerPath = argv[optind];
	else
		headerPath = "version.h";
	size_t hlen = strlen(headerPath);
	if(!get_define) {
/// @todo (White-Tiger#1#): aren't both files doin' "nearly" the same?
		char lockPath[PATH_MAX];
			memcpy(lockPath,headerPath,sizeof(char)*hlen);
			memcpy(lockPath+hlen,".lock",sizeof(char)*6);
		//char incPath[hlen+6];
		//	memcpy(incPath,headerPath,sizeof(char)*hlen);
		//	memcpy(incPath+hlen,".inc",sizeof(char)*5);
		if(g_flag&FLAG_POST) {
			FILE* flock = fopen(lockPath,"rb");
			if(flock){
				fclose(flock);
				unlink(lockPath);
			}
			//flock = fopen(incPath,"wb");
			//if(flock)
			//	fclose(flock);
			return 0;
		} else if(g_flag&FLAG_PRE && g_repo<=REPO_AUTOINC) {
			FILE* flock = fopen(lockPath,"rb");
			if(flock){
				fclose(flock);
				return 0;
			}
			flock = fopen(lockPath,"wb");
			if(flock)
				fclose(flock);
			//flock = fopen(incPath,"rb");
			//if(flock){
			//	fclose(flock);
				//unlink(incPath);
			//}else
			//	do_autoinc=false;
		}
	}
	Version ver = {0};
	ver.date = "unknown date";

//	printf("Version: %u.%hu.%hu.%hu #%u\n",ver.major,ver.minor,ver.build,ver.status,ver.revision);
	ReadHeader(headerPath, ver);
//	printf("Version: %u.%hu.%hu.%hu #%u\n",ver.major,ver.minor,ver.build,ver.status,ver.revision);
	unsigned int rev = ver.revision;
	if(g_repo&REPO_GIT){
		if(QueryGit(Gitpath,&ver))
			g_repo = REPO_GIT;
		else
			g_repo &= ~REPO_GIT;
	}
	if(g_repo&REPO_SVN){
		if(QuerySVN(SVNpath,&ver))
			g_repo = REPO_SVN;
		else
			g_repo &= ~REPO_SVN;
	}
	
	if(get_define) {
		PrintDefine(stdout, get_define, ver);
		return 0;
	}
	
	if(g_repo&REPO_AUTOINC){
		g_repo = REPO_NONE;
		++ver.revision;
	}
//	if(g_repo){ // increase revision because on commit the revision increases :P
//		++ver.revision; // So we should use the "comming" revision and not the previous (grml... date and time is wrong though :/ )
//	}
//	printf("Version: %u.%hu.%hu.%hu #%u\n",ver.major,ver.minor,ver.build,ver.status,ver.revision);
	if(rev!=ver.revision){
		ver.flags_ |= VER_DIRTY;
		puts("	- increased revision");
	}
	if(WriteHeader(headerPath,ver)){
		puts("	- header updated");
	}

	return 0;
}
bool QueryGit(const char* path,Version* ver)
{
	bool found = false;
	char cwd[PATH_MAX]; getcwd(cwd, sizeof(cwd));
	if(chdir(path)){
		puts("invalid repository path!");
		return false;
	}
	char buf[4097],* pos,* data;
	FILE* git;
	git = popen("git rev-list HEAD --count","r");
	if(git){ /// revision count
		int error;
		size_t read = fread(buf,sizeof(char),4096,git); buf[read]='\0'; error=pclose(git);
		if(!error && *buf>='0' && *buf<='9'){ // simple error check on command failure
			ver->revision = atoi(buf);
			git = popen("git remote -v","r");
			if(git){ /// url
				read = fread(buf,sizeof(char),4096,git); buf[read]='\0'; error=pclose(git);
				for(pos=buf; *pos && (*pos!='\r'&&*pos!='\n'&&*pos!=' '&&*pos!='\t'); ++pos);
				for(data=pos; *data=='\r'||*data=='\n'||*data==' '||*data=='\t'; ++data);
				for(pos=data; *pos && (*pos!='\r'&&*pos!='\n'&&*pos!=' '&&*pos!='\t'); ++pos);
				if(!error && *data && pos>data){
					ver->url.assign(data,pos-data);
//					git = popen("git log -1 --pretty=%h%n%an%n%at","r"); // short hash, author, timestamp
					git = popen("git log -1 --pretty=%h%n%at","r"); // short hash, timestamp
					if(git){ /// shorthash,author,timestamp		SVN date example: 2014-07-01 21:31:24 +0200 (Tue, 01 Jul 2014)
						read = fread(buf,sizeof(char),4096,git); buf[read]='\0'; error=pclose(git);
						for(pos=buf; *pos && (*pos!='\r'&&*pos!='\n'); ++pos);
						if(!error && *pos){
							ver->revhash.assign(buf,pos-buf); ++pos;
							time_t tt = atoi(pos);
							tm* ttm = gmtime(&tt);
							strftime(buf,64,"%Y-%m-%d %H:%M:%S +0000 (%a, %b %d %Y)",ttm);
							ver->date.assign(buf);
							found = true;
						}
					}
				}
			}
		}
	}
	chdir(cwd);
	return found;
}
bool QuerySVN(const char* path,Version* ver)
{
	bool found=false;
	string svncmd("svn info --non-interactive ");
	svncmd.append(path);
	FILE* svn = popen(svncmd.c_str(), "r");
	if(svn){
		size_t attrib_len = 0; char attrib[32] = {0};
		size_t value_len = 0; char value[128] = {0};
		char buf[4097];
		size_t read;
		read = fread(buf,sizeof(char),4096,svn); buf[read]='\0';
		if(pclose(svn) == 0){
			for(char* c=buf; *c; ++c) {
				nextloop:
				if(attrib_len >= 31) goto nextline;
				switch(*c) {
				case ':':
					attrib[attrib_len] = '\0';
					for(++c; *c==' ' || *c=='\t'; ++c);
					for(; *c && *c!='\n'; ++c) {
						if(value_len >= 127) {
							value_len = 0; value[0] = '\0';
							break;
						}
						value[value_len++] = *c;
					}
					if(*c == '\n') {
						value[value_len] = '\0';
						if(!strcmp(attrib,"Revision")) {
//							printf("Found: %s %s\n",attrib,value);
							ver->revision = atoi(value);
							found = true;
						} else if(!strcmp(attrib,"Last Changed Date")) {
//							printf("Found: %s @ %s\n",attrib,value);
							ver->date.assign(value);
						} else if(!strcmp(attrib,"URL")) {
//							printf("Found: %s @ %s\n",attrib,value);
							ver->url.assign(value);
						}
					}
					value_len = 0; value[0] = '\0';
				case '\n':
					goto nextline;
				default:
					attrib[attrib_len++] = *c;
				}
				continue;
				nextline:
				for(; *c && *c!='\n'; ++c);
				for(; *c=='\r'||*c=='\n'||*c==' '||*c=='\t'; ++c);
				attrib_len = 0; attrib[0] = '\0';
				if(!*c) break;
				goto nextloop;
			}
		}
	}
	return found;
}
bool ReadHeader(const char* filepath,Version &ver)
{
	FILE* fheader = fopen(filepath,"rb");
	if(!fheader) {
		printf("Error: Couldn't read version file '%s', creating\n",filepath);
		return false;
	}
	unsigned cmajor=0,cminor=0,cbuild=0,cstatus=0;
	char buf[2048] = {0};
	fread(buf,2048,sizeof(char),fheader);
	fclose(fheader);
	size_t def_found, def_num=7;const char def[]="define ";
	size_t attrib_len = 0; char attrib[32] = {0};
	size_t value_len = 0; char value[64] = {0};
	for(char* c=buf; *c; ++c) {
		nextloop:
		if(attrib_len>=31) {attrib_len=0; *attrib='\0'; goto nextline;}
		switch(*c) {
		case '#':
			for(++c; *c==' '||*c=='\t'; ++c);
			for(def_found=0; *c&&*c==def[def_found]; c++,def_found++);
			if(def_found != def_num) goto nextline;
			attrib_len=0; *attrib='\0';
			for(; *c && *c!=' ' && *c!='\n'; attrib[attrib_len++]=*c++);
			if(*c == '\n') goto nextline;
			attrib[attrib_len] = '\0';
			for(++c; *c==' '||*c=='\t'; ++c);
			value_len = 0; *value = '\0';
			for(; *c && *c!='\n'; ++c) {
				if(value_len>=63) break;
				value[value_len++] = *c;
			}
			if(*c == '\n') {
				value[value_len] = '\0';
//				printf("Found: %s: %s\n",attrib,value);
				if(!strcmp(attrib,"VER_MAJOR")) {
					int tmp = atoi(value);
					if(tmp > 0xFF) tmp = 0xFF;
					else if(tmp < 0) tmp = 0;
					ver.major = tmp;
				} else if(!strcmp(attrib,"VER_MINOR")) {
					int tmp = atoi(value);
					if(tmp > 0xFFFF) tmp = 0xFFFF;
					else if(tmp < 0) tmp = 0;
					ver.minor = tmp;
				} else if(!strcmp(attrib,"VER_BUILD")) {
					int tmp = atoi(value);
					if(tmp > 0xFFFF) tmp = 0xFFFF;
					else if(tmp < 0) tmp = 0;
					ver.build = tmp;
				} else if(!strcmp(attrib,"VER_REVISION")) {
					ver.revision = atoi(value);
				} else if(!strcmp(attrib,"VER_STATUS")) {
					int tmp = atoi(value);
					if(tmp > STATUS_NUM_-1) tmp = STATUS_NUM_-1;
					else if(tmp < 0) tmp = 0;
					ver.status = tmp;
				} else if(!strcmp(attrib,"VER_RC_STATUS")) {
					sscanf(value,"%u, %u, %u, %u",&cmajor,&cminor,&cbuild,&cstatus);
				} else if(!strcmp(attrib,"VER_REVISION_URL")) {
					ver.url = value;
					if(ver.url.length() >=2)
						ver.url = ver.url.substr(1,ver.url.length()-2);
				} else if(!strcmp(attrib,"VER_REVISION_DATE")) {
					ver.date = value;
					if(ver.date.length() >=2)
						ver.date = ver.date.substr(1,ver.date.length()-2);
				} else if(!strcmp(attrib,"VER_REVISION_HASH")) {
					ver.revhash = value;
					if(ver.revhash.length() >=2)
						ver.revhash = ver.revhash.substr(1,ver.revhash.length()-2);
				} else if(!strcmp(attrib,"VER_TIMESTAMP")) {
					ver.timestamp = atoi(value);
				}
			}
		default:
			goto nextline;
		}
		continue;
		nextline:
		for(; *c && *c!='\n'; ++c);
		for(; *c=='\r'||*c=='\n'||*c==' '||*c=='\t'; ++c);
		if(!*c) break;
		goto nextloop;
	}
	ver.flags_=0;
	if(cmajor!=ver.major || cminor!=ver.minor || cbuild!=ver.build || cstatus!=ver.status) {
		ver.flags_ |= VER_DIRTY;
	}
	return true;
}
void PrintDefine(FILE* fp,const char* define,const Version &ver)
{
	if(!stricmp("MAJOR",define)) {
		fprintf(fp,"%hu",ver.major);
	}else if(!stricmp("MINOR",define)) {
		fprintf(fp,"%hu",ver.minor);
	}else if(!stricmp("BUILD",define)) {
		fprintf(fp,"%hu",ver.build);
	}else if(!stricmp("STATUS",define)) {
		fprintf(fp,"%hu",ver.status);
	}else if(!stricmp("STATUS_FULL",define)) {
		fprintf(fp,"%s",STATUS_S[ver.status]);
	}else if(!stricmp("STATUS_SHORT",define)) {
		fprintf(fp,"%s",STATUS_SS[ver.status]);
	}else if(!stricmp("STATUS_GREEK",define)) {
		fprintf(fp,"%s",STATUS_SS2[ver.status]);
	}else if(!stricmp("REVISION",define)) {
		fprintf(fp,"%u",ver.revision);
	
	// version strings
	}else if(!stricmp("FULL",define)) {
		if(STATUS_S[ver.status][0])
			fprintf(fp,"%hu.%hu.%hu %s",ver.major,ver.minor,ver.build,STATUS_S[ver.status]);
		else
			fprintf(fp,"%hu.%hu.%hu",ver.major,ver.minor,ver.build);
	}else if(!stricmp("SHORT",define)) {
		fprintf(fp,"%hu.%hu%s%hu",ver.major,ver.minor,STATUS_SS[ver.status],ver.build);
	}else if(!stricmp("SHORT_DOTS",define)) {
		fprintf(fp,"%hu.%hu.%hu",ver.major,ver.minor,ver.build);
	}else if(!stricmp("SHORT_GREEK",define)) {
		fprintf(fp,"%hu.%hu%s%hu",ver.major,ver.minor,STATUS_SS2[ver.status],ver.build);
	}else if(!stricmp("RC_REVISION",define)) {
		fprintf(fp,"%hu, %u, %u, %u",ver.major,ver.minor,ver.build,ver.revision);
	}else if(!stricmp("RC_STATUS",define)) {
		fprintf(fp,"%hu, %u, %u, %u",ver.major,ver.minor,ver.build,ver.status);
	
	// repository
	}else if(!stricmp("REVISION_URL",define)) {
		fprintf(fp,"%s",ver.url.c_str());
	}else if(!stricmp("REVISION_DATE",define)) {
		fprintf(fp,"%s",ver.date.c_str());
	}else if(!stricmp("REVISION_HASH",define)) {
		fprintf(fp,"%s",ver.revhash.c_str());
	}else if(!stricmp("REVISION_TAG",define)) {
		fprintf(fp,"v%hu.%hu.%hu#%hu",ver.major,ver.minor,ver.build,ver.revision);
		if(STATUS_S[ver.status][0]){
			fputc('-',fp);
			for(const char* c=STATUS_S[ver.status]; *c; ++c){
				if(*c == ' ')
					fputc('_',fp);
				else
					fputc(tolower(*c),fp);
			}
		}
	
	// date / time
	}else if(!stricmp("TIMESTAMP",define)) {
		fprintf(fp,"%lu",ver.timestamp);
	
	
	}else{
		fprintf(fp,"unknown define: %s\n",define);
	}
}
void WriteDefine(FILE* fp,const char* define,const Version &ver)
{
	fprintf(fp,"#	define VER_%s ",define);
	PrintDefine(fp,define,ver);
	putc('\n',fp);
}
void WriteDefineString(FILE* fp,const char* define,const Version &ver)
{
	fprintf(fp,"#	define VER_%s \"",define);
	PrintDefine(fp,define,ver);
	fputs("\"\n",fp);
}
bool WriteHeader(const char* filepath,Version &ver)
{
	FILE* fheader;
	if(!(ver.flags_&VER_DIRTY)){
		return false;
	}
	fheader = fopen(filepath,"wb");
	if(!fheader) {
		puts("Error: Couldn't open output file.");
		return false;
	}
	fputs("/* Note: to use integer defines as strings, use for example STR(VER_REVISION) */\n",fheader);
	fputs("#pragma once\n",fheader);
	fputs("#ifndef AUTOVERSION_H\n",fheader);
	fputs("#define AUTOVERSION_H\n",fheader);
	fputs("#	define XSTR(x) #x\n",fheader);
	fputs("#	define STR(x) XSTR(x)\n",fheader);
//	fputs("namespace Version{\n",fheader);
	fputs("/** Version **/\n",fheader);
	WriteDefine(fheader,"MAJOR",ver);
	WriteDefine(fheader,"MINOR",ver);
	WriteDefine(fheader,"BUILD",ver);
	fprintf(fheader,"	/* status values: 0=%s",STATUS_S[0]);
	for(int i=1; i<STATUS_NUM_; ++i)
		fprintf(fheader,", %i=%s",i,STATUS_S[i]);
	fputs(" */\n",fheader);
	WriteDefine(fheader,"STATUS",ver);
	WriteDefineString(fheader,"STATUS_FULL",ver);
	WriteDefineString(fheader,"STATUS_SHORT",ver);
	WriteDefineString(fheader,"STATUS_GREEK",ver);
	WriteDefine(fheader,"REVISION",ver);
	
	WriteDefineString(fheader,"FULL",ver);
	WriteDefineString(fheader,"SHORT",ver);
	WriteDefineString(fheader,"SHORT_DOTS",ver);
	WriteDefineString(fheader,"SHORT_GREEK",ver);
	WriteDefine(fheader,"RC_REVISION",ver);
	WriteDefine(fheader,"RC_STATUS",ver);
	if(g_repo) {
		fputs("/** Subversion Information **/\n",fheader);
		WriteDefineString(fheader,"REVISION_URL",ver);
		WriteDefineString(fheader,"REVISION_DATE",ver);
		WriteDefineString(fheader,"REVISION_HASH",ver);
		// revision tag
		WriteDefineString(fheader,"REVISION_TAG",ver);
	}
	char tmp[64];
	time(&ver.timestamp);
	tm* ttm=gmtime(&ver.timestamp);
	fputs("/** Date/Time **/\n",fheader);
	WriteDefine(fheader,"TIMESTAMP",ver);
	fprintf(fheader,"#	define VER_TIME_SEC %i\n",ttm->tm_sec);
	fprintf(fheader,"#	define VER_TIME_MIN %i\n",ttm->tm_min);
	fprintf(fheader,"#	define VER_TIME_HOUR %i\n",ttm->tm_hour);
	fprintf(fheader,"#	define VER_TIME_DAY %i\n",ttm->tm_mday);
	fprintf(fheader,"#	define VER_TIME_MONTH %i\n",ttm->tm_mon+1);
	fprintf(fheader,"#	define VER_TIME_YEAR %i\n",1900+ttm->tm_year);
	fprintf(fheader,"#	define VER_TIME_WDAY %i\n",ttm->tm_wday);
	fprintf(fheader,"#	define VER_TIME_YDAY %i\n",ttm->tm_yday);
	strftime(tmp,64,"%a",ttm);
	fprintf(fheader,"#	define VER_TIME_WDAY_SHORT \"%s\"\n",tmp);
	strftime(tmp,64,"%A",ttm);
	fprintf(fheader,"#	define VER_TIME_WDAY_FULL \"%s\"\n",tmp);
	strftime(tmp,64,"%b",ttm);
	fprintf(fheader,"#	define VER_TIME_MONTH_SHORT \"%s\"\n",tmp);
	strftime(tmp,64,"%B",ttm);
	fprintf(fheader,"#	define VER_TIME_MONTH_FULL \"%s\"\n",tmp);
	strftime(tmp,64,"%H:%M:%S",ttm);
	fprintf(fheader,"#	define VER_TIME \"%s\"\n",tmp);
	strftime(tmp,64,"%Y-%m-%d",ttm);
	fprintf(fheader,"#	define VER_DATE \"%s\"\n",tmp);
	strftime(tmp,64,"%a, %b %d, %Y %H:%M:%S UTC",ttm);
	fprintf(fheader,"#	define VER_DATE_LONG \"%s\"\n",tmp);
	strftime(tmp,64,"%Y-%m-%d %H:%M:%S UTC",ttm);
	fprintf(fheader,"#	define VER_DATE_SHORT \"%s\"\n",tmp);
	strftime(tmp,64,"%Y-%m-%dT%H:%M:%SZ",ttm);
	fprintf(fheader,"#	define VER_DATE_ISO \"%s\"\n",tmp);

	fputs("#endif\n",fheader);
	fclose(fheader);
	return true;
}
