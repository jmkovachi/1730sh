#define _POSIX_SOURCE

#include <iostream>
#include <string.h>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <vector>
#include <algorithm>
#include <fcntl.h>

using namespace std;

/**
Struct used to organize our processes
*/
struct Process
{
    vector<string> args;
    int PID;
    int status;
};

extern const char * const sys_siglist[];
void close_pipe(int pipefd[2]);

string sigdesc;
int exitstatus = 0;
int pgidcount = 0;
//bool background = false;

/**
makes a string upper-case
@param s c-string
*/
void upcase(char *s)
{
    while (*s)
    {
        *s = toupper(*s);
        s++;        
    }
}

/**
redirects output
@param argv vector of string
@param argc number of args
return index of redirection
*/
int redirect(vector<string> argv,int argc) {
	cout.setf(ios::unitbuf);
	int redirectindex = -1;
	int redirectcount = 0;
	for (int i = 0; i < argc; i++) {
		if (argv[i] == "<" || argv[i] == ">" || argv[i] == ">>" || argv[i] == "e>>") {
			if (redirectcount == 0) {
				redirectindex = i;
			}
			if (argv[i] == "<" && i < argc-1) {
				int n = open(argv[i+1].c_str(),O_RDWR);
				if (n == -1) {
					cout << argv[i+1] << " does not exist" << endl;
					return -1;
				}
				dup2(n,STDIN_FILENO);
				close(n);
			}
			else if (argv[i] == ">" && i < argc-1) {
				int n = open(argv[i+1].c_str(),O_WRONLY|O_CREAT,0666);
				if (n == -1) {
					//cout << "HI" << endl;
					perror("open");
					return -1;
				}
				dup2(n,STDOUT_FILENO);
				close(n);
			}
			else if (argv[i] == ">>" && i < argc-1) {
				int n;
				if( access(argv[i+1].c_str(), F_OK ) != -1 ) {
					n = open(argv[i+1].c_str(),O_WRONLY|O_APPEND,0666);
				} else {
					n = open(argv[i+1].c_str(),O_WRONLY|O_CREAT,0666);
				}
				if (n == -1) {
					perror("open");
					return -1;
				}
				dup2(n,STDOUT_FILENO);
				close(n);
			}
			else if (argv[i] == "e>>" && i < argc-1){
				int n;
				if( access(argv[i+1].c_str(), F_OK ) != -1 ) {
					n = open(argv[i+1].c_str(),O_WRONLY|O_APPEND,0666);
				} else {
					n = open(argv[i+1].c_str(),O_WRONLY|O_CREAT,0666);
				}
				if (n == -1) {
					//cout << "HI" << endl;
					perror("open");
					return -1;
				}
				dup2(n,STDERR_FILENO);
				close(n);
			}
			else if (argv[i] == "e>" && i < argc-1) {
				int n = open(argv[i+1].c_str(),O_WRONLY|O_CREAT,0666);
				if (n == -1) {
					perror("open");
					return -1;
				}
				dup2(n,STDERR_FILENO);
				close(n);
			}
			redirectcount++;
		}
	}
	if (redirectindex == -1) {
		redirectindex = argc;
	}
	return redirectindex;
}

/*
checks if a specified string is an int
@param argv string
@param length length of string
return true or false
*/
bool checkdigits(const char * argv, int length) {
  for (int i = 0; i < length; i++) {
    if (!isdigit(argv[i])) {
      return false;
    }
  }
  return true;
}

/*void child_handler(int signo) {
	cout << "HI" << endl;
	if (signo == SIGINT) {
		kill(getpid(),SIGINT);
		sigdesc = "SIGINT";
	}
}*/

/**
Signal handler
@param signo number of signal
*/
void sig_handler(int signo)
{
    if (signo == SIGINT){
		if (getpid() == 0) {
			sigdesc = "SIGINT";
			kill(getpid(),SIGINT);
		}
        sigdesc = "SIGINT";
	}
    else if (signo == SIGKILL)
        sigdesc = "SIGKILL";
    else if (signo == SIGTSTP) {
		//cout << "HI" << endl;
		sigdesc = "SIGTSTP";}	
	else if (signo == SIGTTIN)
		sigdesc = "SIGTTIN";
	else if (signo == SIGTTOU)
		sigdesc = "SIGTTOU";
	else if (signo == SIGCHLD) {
		//cout << "HI" << endl;
		//sigdesc = "SIGCHLD";
	}
	else if (signo == SIGQUIT) {
		sigdesc = "SIGQUIT";
	}
	else {
		sigdesc = strsignal(signo);
	}
}


/**
Finds a string that is enclosed in quotes inside of a string. Pretty handy method for parsing a string
@param s passed in string
return parsed string
*/
string findquotes(string s) {
	string newstring;
	for (unsigned int i = 0; i < s.length(); i++) {
		if (s[i] == '"') {
			//int charcount = 0;
			if (i != s.length()-1) {
				while (s[i+1] != '"' && s[i+1] != '\\') {
					newstring += s[i+1];
					if (i+1 == s.length()) {
						break;
					}
					i++;
				}
				if (i != s.length()-2) {
					if(s[i+1] == '\\' && s[i+2] == '"') {
						newstring += '"';
					}
				}
				if (s[i] == '"') break;
			}
		}
	}
	return newstring;
}

/*
Returns specified index of process group in vector
@param pid PGID
@param provec process vector
*/
int pgindex(int pid, vector<Process> & provec) {
	for (unsigned int i = 0; i < provec.size(); i++) {
		if (getpgid(pid) == provec[i].PID) {
			return i;
		}
		if (provec[i].PID == pid) return i;
	}
	return 0;
}

/*
Returns a string of the vector args
@param s vector of arguments
return complete string
*/
string toString(vector<string> s) {
	string newstring;
	for (unsigned int i = 0; i < s.size(); i++) {
		newstring += s[i] += " ";
	}
	return newstring;
} 

/**
Handles the main logic for parsing the string and outputting output
*/
int main(int argn, char * argb[], char * envp[]) {
	
	cout.setf(ios::unitbuf);
	vector<Process> provec;
	//int realpgid = tcgetpgrp(STDIN_FILENO);
	while (true) {
		//cout << getpid() << endl;
		
		int newstatus;
		int wp;
		signal(SIGINT,sig_handler);
		signal(SIGQUIT,sig_handler);
		//signal(SIGKILL,sig_handler);
		signal(SIGTSTP,sig_handler);
		signal(SIGTTIN,sig_handler);
		signal(SIGTTOU,sig_handler);
		signal(SIGTTIN,sig_handler);
		signal(SIGCHLD,sig_handler);
		while ((wp = waitpid(-1,&newstatus, WUNTRACED|WCONTINUED|WNOHANG)) > 0) {
			//cout << "ENTER" << endl;
			provec[pgindex(wp,provec)].status = newstatus;
			if (WIFEXITED(newstatus)) {
						exitstatus = WEXITSTATUS(newstatus);
					  cout << provec[pgindex(wp,provec)].PID << " " << "Exited ("  << WEXITSTATUS(newstatus) << ")\t" << toString(provec[pgindex(wp,provec)].args) << endl;
					  provec.erase(provec.begin() + pgindex(wp,provec)); //erase specific job
						   break;
						} else if (WIFSTOPPED(newstatus)) {
							//provec.push_back(newpro);
							
						  cout << endl << provec[pgindex(wp,provec)].PID << " " << "Stopped\t" << toString(provec[pgindex(wp,provec)].args) << endl;
						  //kill(wpid, SIGCONT);
						  break;
						} else if (WIFCONTINUED(newstatus)) {
						  cout << endl << provec[pgindex(wp,provec)].PID << " " << "Continued\t" << toString(provec[pgindex(wp,provec)].args) << endl;
						} else if (WIFSIGNALED(newstatus)) {
							provec.erase(provec.begin() + pgindex(wp,provec)); //erase specific job
							exitstatus = WEXITSTATUS(newstatus);
							 //int sig = WTERMSIG(pstatus);
							cout << endl << wp << " " << "Exited ("  << sigdesc << ")\t" << toString(provec[pgindex(wp,provec)].args) << endl;
						   break;
			} // if	
			//cout << pgindex(wp,provec) << endl;
			provec[pgindex(wp,provec)].status = newstatus;
		}
		int stdinnum = dup(0);
		int stdoutnum = dup(1);
		int stderrnum = dup(2);
		//cout << "yo" << endl;
		
		int processcount = 1;
		int pipecount = 0;
		string standardin = "STDIN_FILENO";
		string standardout = "STDOUT_FILENO";
		string standarderr = "STDERR_FILENO";
		
		/* Code for prompt */
		char path [255];
		realpath(".", path);
		string hpath(path);
		string home = getenv("HOME");
		size_t index = hpath.find(getenv("HOME"));
		cout << "1730sh:";
		if(index != string::npos)
		{
			cout << "~";
			hpath = hpath.substr(index + home.length(), string::npos);
			cout << hpath;
		}
		else
		{
			for(unsigned int i = 5; i < hpath.length(); i++)
				cout << hpath[i];
		}
		cout << "/$ ";
		/* end of code for prompt */
	//	tcsetpgrp(1,realpgid);
		string s = "";
		getline(cin,s);
		if (std::cin.eof()==1) {
			cout << endl;
			break;
		}
		if(s == "") continue;
		//cout << "HI" << endl;
		if (s == "list") {
			for (unsigned int i = 0; i < provec.size(); i++) {
				for (unsigned int j = 0; j < provec[i].args.size(); j++) {
					cout << provec[i].args[j] << " ";
					//cout << getpgid(provec[i].PID) << endl;
					
				}
				if (provec[i].PID == getpgid(provec[i].PID)) {
						cout << "THIS works" << endl;
				}
				cout << endl;
			}
			continue;
		}
		
		string newstring = "";
		unsigned int q = 0;
		while (s[q] != '"') {
			//newstring[q] += s[q];
			q++;
			if (q == s.length()) break;
			//cout << newstring << endl;
		}
		if (q != s.length() && s[q] == '"') {
			newstring += findquotes(s);
		}
		string nstring = s.substr(0,q);
		s = nstring + newstring;
		
		stringstream ss(s);
		string argv[100];
		int argc = 0;
		int processindex[1000];
		while (ss.good()) {
			ss >> argv[argc];
			argc++;
		}
		
		if (s == "jobs") {
			if (provec.size() > 0) {
				cout << "JID  " << "STATUS\t\t" << "COMMAND\n";
			}
			for (unsigned int i = 0; i < provec.size(); i++) {
				cout << provec[i].PID << " ";
				if (provec[i].status == -100) {
					cout << "RUNNING\t\t";
				}
				else if (WIFSTOPPED(provec[i].status)) {
					cout << "STOPPED\t\t";
				}
				/*else if (WIFCONTINUED(provec[i].stat)) {
					cout << "CONTINUED\t\t";
				}
				else if (WIFEXITED(provec[i].status) {
					cout << "EXITED\t\t";
				}*/
				else if (provec[i].status == 0) {
					cout << "STOPPED\t\t";
				}
				else {
					cout << "STATUS" << provec[i].status << endl;
					cout << "RUNNING\t\t";
				}
				for (unsigned int j = 0; j < provec[i].args.size(); j++) {
					cout << provec[i].args[j] << " ";
					//cout << getpgid(provec[i].PID) << endl;
					
				}
				cout << endl;
			}
			continue;
		}
		else if(argv[0] == "fg"){//builtin                                                 
		  int jid = stoi(argv[1]);
		  int jkill;
		 
		  //setpgid(jid,getpgrp());
		 /* signal(SIGINT,SIG_DFL);
			signal(SIGQUIT,SIG_DFL);
			signal(SIGKILL,SIG_DFL);
			signal(SIGTSTP,SIG_DFL);
			signal(SIGTTIN,SIG_DFL);
			signal(SIGTTOU,SIG_DFL);
			signal(SIGTTIN,SIG_DFL);
			signal(SIGCHLD,SIG_DFL);*/
		  if((jkill = kill(jid, SIGCONT)) == -1){
			cout << "Could not continue Job" << endl;
		  }
		 // tcsetpgrp(STDIN_FILENO,jid);
		  //tcsetpgrp(jid);
		  //tcsetpgrp(STDOUT_FILENO,jid);  /*ASK ABOUT SIGNALS CONCERNING FG*/
		  //tcsetpgrp(STDERR_FILENO,jid);
		  if ((wp = waitpid(jid,&newstatus, WUNTRACED)) > 0) {
			 // cout << "HI" << endl;
		//	cout << "ENTER" << endl;
			//cout << "INDEX" << pgindex(jid,provec) << endl;
			//cout << "INDEX: " << pgindex()
				provec[pgindex(jid,provec)].status = newstatus;
				if (WIFEXITED(newstatus)) {
							exitstatus = WEXITSTATUS(newstatus);
							cout << provec[pgindex(jid,provec)].PID << " " << "Exited ("  << WEXITSTATUS(newstatus) << ")\t" << toString(provec[pgindex(jid,provec)].args) << endl;
							//cout << "EXIT" << endl;
							provec.erase(provec.begin() + pgindex(wp,provec)); //erase specific job
							 //  break;
						} else if (WIFSTOPPED(newstatus)) {
								//provec.push_back(newpro);
								
							  cout << endl << provec[pgindex(jid,provec)].PID << " " << "Stopped\t" << toString(provec[pgindex(jid,provec)].args) << endl;
							  //kill(wpid, SIGCONT);
							 // break;
						} else if (WIFCONTINUED(newstatus)) {
							  cout << endl << provec[pgindex(jid,provec)].PID << " " << "Continued\t" << toString(provec[pgindex(jid,provec)].args) << endl;
							} else if (WIFSIGNALED(newstatus)) {
								exitstatus = WEXITSTATUS(newstatus);
								 //int sig = WTERMSIG(pstatus);
								cout << endl << provec[pgindex(jid,provec)].PID << " " << "Exited ("  << sigdesc << ")\t" << toString(provec[pgindex(jid,provec)].args) << endl;
								//cout << "SIG" << endl;
								provec.erase(provec.begin() + pgindex(wp,provec)); //erase specific job
								//cout << "SIG2" << endl;
							 //  break;
				} // if	
				//cout << pgindex(wp,provec) << endl;
				provec[pgindex(wp,provec)].status = newstatus;
			}
			//cout << "control" << endl;
			//tcsetpgrp(STDIN_FILENO,realpgid);
			//cout << "lol" << endl;
		  continue;
		}
		else if(argv[0] == "help" && argc == 1){
			  cout << "Builtin Commands:\n" << endl;
			  cout << "$ cd [PATH]           - Change the current directory to PATH. The environment variable\n\t\t\t HOME is the default PATH." << endl;
			  cout << "\n$ exit [N]            - Cause the shell to exit with a status of N. If N is omitted, the\n\t\t\texit status is that of the last job executed." << endl;
			  cout << "\n$ help                - Display helpful information about builtin commands." << endl;
			  cout << "\n$ bg JID              - Resume the stopped job JID in the background, as if it had been\n\t\t\tstarted with &." << endl;
			  cout << "\n$ export NAME[=WORD]  - NAME is automatically included in the environment of subsequently \n\t\t\texecuted jobs." << endl;
			  cout << "\n$ fg JID              - Resume job JID in the foreground, and make it the current job." << endl;
			  cout << "\n$ jobs                - List current jobs. " << endl;
			  cout << "\n$ kill                - The kill utility sends the specied signal to the specied process\n\t\t\tor process group PID" << endl;
			  continue;
		}
		else if(argv[0] == "export")
		{             
			if((argc == 1) | (argc > 2))
			{
				cout << "export: invalid number of arguements" << endl;
				continue;
			}
			else
			{
				string name = "NAME=";
				string arg = argv[1];
				size_t index = arg.find(name);
				if(index != string::npos)
				{
					arg = arg.substr(name.length(), string::npos);
					setenv("NAME", arg.c_str(), 1);
				}
				else
				{
					cout << "export: invalid arguement `" << arg << "'" << endl;
					continue;
				}
			}
			continue;
		}
		else if(argv[0] == "exit"){//builtin                                                                        
		  if(argc == 1){//no -N                                                                                     
			_exit(exitstatus);
		  }
		  else{//has -N                                                                                             
			int isnum = 0;
			for(unsigned int i = 0; i < argv[1].length();i++){
			  if(!(isdigit(argv[1][i]))){
				isnum = 1;
			  }
			  if(isnum == 0){
				string schar = argv[1];
				int statu = stoi(schar,nullptr,0);
				//cout << "Exit status num: " << statu << endl;                                                     
				_exit(statu);
			  }
			  else
				cout << "Invalid -N option" << endl;
			}
		  }
		  continue;
		}
		
		Process newpro; //create new process struct
		
		
		for (int i = 0; i < argc; i++) {
			newpro.args.push_back(argv[i]);
		}
		//provec.push_back(newpro);
		if (argv[0] == "quit" && argc == 1) {
			return exitstatus;
		}
		else if (argv[0] == "quit" && checkdigits(argv[1].c_str(),argv[1].length()) && argc == 2) {
			return atoi(argv[1].c_str());
		}
		else if ((argv[0] == "quit") | ((argv[0] == "help") & (argc > 1)) | ((argv[0] == "cd") & (argc > 2))) {
			cout << "Too many arguments" << endl;
			continue;
		}
		else if(argv[0] == "help" && argc == 1){
			cout << "Builtin Commands:\n" << endl;
			cout << "$ cd [PATH] - Change the current directory to PATH. The environment variable\n\t      HOME is the default PATH." << endl;
			cout << "\n$ exit [N]  - Cause the shell to exit with a status of N. If N is omitted, the\n\t      exit status is that of the last job executed." << endl;
			cout << "\n$ help      - Display helpful information about builtin commands." << endl;
			continue;
		}
		else if(argv[0] == "cd")
		{
			string path = argv[1];
			string default_path = getenv("HOME");

			if(path == "" || path == "~")
			  if(default_path.c_str() != NULL)
				path = default_path;

			if(chdir(path.c_str()) < 0)
			  cout << "cd: " << path << ": " << strerror(errno) << endl;

			continue;
		}
		else if(argv[0] == "bg"){//builtin                                                                          
		  int jid = stoi(argv[1]);
		  int jkill;
		  if((jkill = kill(jid, SIGCONT)) == -1){
			cout << "Could not continue Job" << endl;
		  }
		  setpgid(jid,0);
		  continue;
		}
		else if(argv[0] == "kill"){//builtin                                                 
		  if(argc != 2 && argc != 4){
			cout << "Invalid number of parameters" << endl;
			continue;
		  }
		  int sigid;//signal to send                                                         
		  int ipid;//pid to send signal to                                                   
		  int pkill;//return val of kill                                                     
		  int cer = 0;//certify stoi                                                         
		  if(argc == 2){//default                                                            
			sigid = 9; 
			//cout << argv[1] << endl;                                                       
			for(unsigned int i = 0; i < argv[1].length();i++){
			  if(isdigit(argv[1].at(i)) == 0){
				cout << "kill: " << argv[1] << " - invalid signal argument" << endl;
				cer = 1;
			  }
			}
			if(cer == 1)
			  continue;
			ipid = stoi(argv[1]);
		  }
		  else{//-s SIG                                                                      
			for(unsigned int i = 0;i < argv[3].length();i++){
			  if(isdigit(argv[3].at(i)) == 0){
				cout << "kill: " << argv[3] << " - invalid signal argument" << endl;
				cer = 1;
			  }
			}
			if(cer == 1)
			  continue;
			ipid = stoi(argv[3]);
			if(argv[1] != "-s"){
			  cout << "kill: " << argv[1] << " - invalid signal argument" << endl;
			  continue;
			}
			if(argv[2] == "SIGHUP")
			  sigid = 1;
			else if(argv[2] == "SIGINT")
			  sigid = 2;
			else if(argv[2] == "SIGQUIT")
			  sigid = 3;
			else if(argv[2] == "SIGILL")
			  sigid = 4;
			else if(argv[2] == "SIGABRT")
			  sigid = 6;
			else if(argv[2] == "SIGFPE")
			  sigid = 8;
			else if(argv[2] == "SIGKILL")
			  sigid = 9;
			else if(argv[2] == "SIGSEGV")
			  sigid = 11;
			else if(argv[2] == "SIGPIPE")
			  sigid = 13;
			else if(argv[2] == "SIGALRM")
			  sigid = 14;
			else if(argv[2] == "SIGTERM")
			  sigid = 15;
			else if(argv[2] == "SIGCHLD")
			  sigid = 17;
			else if(argv[2] == "SIGCONT") {
			  sigid = 18;
			}
			else if(argv[2] == "SIGSTOP")
			  sigid = 19;
			else if(argv[2] == "SIGTSTP")
			  sigid = 20;
			else if(argv[2] == "SIGTTIN")
			  sigid = 21;
			else if(argv[2] == "SIGTTOU")
			  sigid = 22;
			else{
			  cout << "kill: " << argv[2] << " - invalid signal argument" << endl;
			  continue;
			}
		  }
			if((pkill = kill(ipid, sigid)) == -1){
			  cout << "kill: " << ipid << " -  no such process" << endl;
			}
			 if ((wp = waitpid(ipid,&newstatus, WUNTRACED)) > 0) {
				// cout << "HI" << endl;
				//cout << "ENTER" << endl;
				//cout << "INDEX" << pgindex(jid,provec) << endl;
				//cout << "INDEX: " << pgindex()
				provec[pgindex(ipid,provec)].status = newstatus;
				if (WIFEXITED(newstatus)) {
							exitstatus = WEXITSTATUS(newstatus);
							cout << provec[pgindex(ipid,provec)].PID << " " << "Exited ("  << WEXITSTATUS(newstatus) << ")\t" << toString(provec[pgindex(ipid,provec)].args) << endl;
							//cout << "EXIT" << endl;
							provec.erase(provec.begin() + pgindex(wp,provec)); //erase specific job
							 //  break;
						} else if (WIFSTOPPED(newstatus)) {
								//provec.push_back(newpro);
								
							  cout << endl << provec[pgindex(ipid,provec)].PID << " " << "Stopped\t" << toString(provec[pgindex(ipid,provec)].args) << endl;
							  //kill(wpid, SIGCONT);
							 // break;
						} else if (WIFCONTINUED(newstatus)) {
							  cout << endl << provec[pgindex(ipid,provec)].PID << " " << "Continued\t" << toString(provec[pgindex(ipid,provec)].args) << endl;
							} else if (WIFSIGNALED(newstatus)) {
								exitstatus = WEXITSTATUS(newstatus);
								 //int sig = WTERMSIG(pstatus);
								cout << endl << provec[pgindex(ipid,provec)].PID << " " << "Exited ("  << sigdesc << ")\t" << toString(provec[pgindex(ipid,provec)].args) << endl;
								//cout << "SIG" << endl;
								provec.erase(provec.begin() + pgindex(wp,provec)); //erase specific job
								//cout << "SIG2" << endl;
							 //  break;
				} // if	
				//cout << pgindex(wp,provec) << endl;
				provec[pgindex(wp,provec)].status = newstatus;
		  continue;
		}
		}
		
		for (int i = 0; i < argc; i++) {
			if (argv[i].compare(">") == 0) {
				standardout = argv[i+1] + " (truncate)";
				i++;
			}
			else if (argv[i].compare(">>") == 0) {
				standardout = argv[i+1] + " (append)";
				i++;
			}
			else if (argv[i].compare("<") == 0) {
				standardin = argv[i+1];
				i++;
			}
			else if (argv[i].compare("e>>") == 0) {
				standarderr = argv[i+1] + " (append)";
				i++;
			}
			else if (argv[i].compare("e>") == 0) {
				standarderr = argv[i+1];
				i++;
			}
			else if (argv[i].compare("|") == 0) {
				processindex[processcount] = i;
				processcount++;
				pipecount++;
				
			}
		}
		
		processindex[processcount] = argc - 1 - processindex[processcount-1];
		
		//Creating the 2D vector array********************************
		int pipe_count = 0;
		bool already_piped = true;
		vector<int> pipe_location;
		string arg;
		for(int i = 0; i < argc; i++)
		{
			arg = argv[i];
			if(arg == "|")
			{
				pipe_count++;
				pipe_location.push_back(i);
			}
		}
		
		if(pipe_count > 0)
		{
			already_piped = false;

			vector<vector<string>> processes;
			processes.resize(pipe_count + 1); // height of 2D vector array
			
			
			vector<int> process_size;
			for(unsigned int i = 0; i <= pipe_location.size(); i++)
				process_size.push_back(-1);
			
			for(unsigned int i = 0; i < process_size.size(); i++)
			{
				if(i == 0)
					process_size[i] = pipe_location[i];
				else if(i == (process_size.size() - 1))
					process_size[i] = argc - 1 - pipe_location[pipe_location.size() - 1];
				else
					process_size[i] = pipe_location[i] - pipe_location[i - 1] - 1;
			}
			/*
			for(unsigned int i = 0; i < process_size.size(); i++)
				cout << "process_size[" << i << "]: " << process_size[i] << endl;
			*/
			for(int i = 0; i < (pipe_count + 1); i++)
				processes[i].resize(process_size[i]); // width of 2D vector array
			
			int pos = 0;
			for(unsigned int i = 0; i < processes.size(); i++) //rows
			{
				for(unsigned int j = 0; j < processes[i].size(); j++) //cols
				{
					arg = argv[pos];
					if(arg == "|")
					{
						j--;
						pos++;
						continue;
					}
					processes[i][j] = argv[pos];
					//cout << processes[i][j] << " ";
					pos++;
				}
				//cout << endl;
			}
			//end creating 2D vector array

		

			//being piping
			if(pipe_count == 1)
			{
				int pipefd[2];
				int pipe_id, pipe_id2;
				bool bg_process = false;
				int redirectindex = redirect(processes[0],processes[0].size());
				int rows = processes.size()-1;
				int cols = processes[rows].size()-1; 
				//cerr << "rows: " << rows << endl;
				//cerr << "cols: " << cols << endl;
				//cerr << "last process: " << processes[rows][cols] << endl;
				if(processes[rows][cols] == "&")
				{
					bg_process = true;
					//redirectindex--;
				}
				
				if(pipe(pipefd) == -1)
					perror("pipe");
				if((pipe_id = fork()) == -1)
					perror("fork");
				if(pipe_id != 0)
					newpro.PID = pipe_id;
				else if(pipe_id == 0)
				{
					if(bg_process)
						setpgid(0,0);
					 
					if(dup2(pipefd[1], STDOUT_FILENO) == -1)
						perror("dup2");
					
					close_pipe(pipefd);
					
					char ** argz = new char * [redirectindex + 1];
					for(int j = 0; j < redirectindex; j++)
						argz[j] = strdup(processes[0][j].c_str());
					argz[redirectindex] = nullptr;
					
					if (redirectindex == -1) {
						//cout << "WWW" << endl;
						dup2(stdinnum,STDIN_FILENO);
						dup2(stdoutnum,STDOUT_FILENO);
						exit(EXIT_FAILURE);
						//continue;
					}
					
					
					execvp(argz[0], argz);
					
					perror("1730sh");
					for(int j = 0; j < (redirectindex + 1); j++)
						free(argz[j]);
					delete[] argz;
					exit(EXIT_FAILURE);
				}
				

				if((pipe_id2 = fork()) == -1)
					perror("fork");
				if(pipe_id2 != 0)
					newpro.PID = pipe_id2;
				else if(pipe_id2 == 0)
				{
					int redirectindex = redirect(processes[1],processes[1].size());
					
					if(bg_process)
					{
						setpgid(0,0);
						redirectindex--;
					}
					
					if(dup2(pipefd[0], STDIN_FILENO) == -1)
						perror("dup2");
					
					close_pipe(pipefd);
					
					if (redirectindex == -1) {
						//cout << "WWW" << endl;
						dup2(stdinnum,STDIN_FILENO);
						dup2(stdoutnum,STDOUT_FILENO);
						exit(EXIT_FAILURE);
						//continue;
					}
					
					char ** argz = new char * [redirectindex + 1];
					for(int j = 0; j < redirectindex; j++)
						argz[j] = strdup(processes[1][j].c_str());
					argz[redirectindex] = nullptr;
					
					//cout << argz[0] << endl;
					
					
					
					execvp(argz[0], argz);
					
					perror("1730sh");
					for(int j = 0; j < (redirectindex + 1); j++)
						free(argz[j]);
					delete[] argz;
					exit(EXIT_FAILURE);
				}
				close_pipe(pipefd);
				//waitpid(pipe_id, nullptr, 0);
				pid_t wpid;
				int pstatus;
				
				  while ((wpid = waitpid(pipe_id, &pstatus, /*WNOHANG |*/ WUNTRACED | WCONTINUED)) > 0) {
					if (WIFEXITED(pstatus)) {
						exitstatus = WEXITSTATUS(pstatus);
						//cout << wpid << " " << "Exited ("  << WEXITSTATUS(pstatus) << ")\t" << argv[0] << endl;
					   break;
					} else if (WIFSTOPPED(pstatus)) {
						//cout << endl << wpid << " " << "Stopped\t" << argv[0] << endl;
					  //kill(wpid, SIGCONT);
					  break;
					} else if (WIFCONTINUED(pstatus)) {
						//cout << endl << wpid << " " << "Continued\t" << argv[0] << endl;
					} else if (WIFSIGNALED(pstatus)) {
						exitstatus = WEXITSTATUS(pstatus);
						 //int sig = WTERMSIG(pstatus);
						//cout << endl << wpid << " " << "Exited ("  << sigdesc << ")\t" << argv[0] << endl;
					   break;
					} // if
					//sleep(1);
				  } // while
				if(!bg_process)
				{
				   while ((wpid = waitpid(pipe_id2, &pstatus, /*WNOHANG |*/ WUNTRACED | WCONTINUED)) > 0) {
					   newpro.status = pstatus;
					if (WIFEXITED(pstatus)) {
						exitstatus = WEXITSTATUS(pstatus);
						cout << newpro.PID << " " << "Exited ("  << WEXITSTATUS(pstatus) << ")\t" << toString(newpro.args) << endl;
					   break;
					} else if (WIFSTOPPED(pstatus)) {
						provec.push_back(newpro);
						cout << endl << newpro.PID << " " << "Stopped\t" << toString(newpro.args) << endl;
					  //kill(wpid, SIGCONT);
					  break;
					} else if (WIFCONTINUED(pstatus)) {
						cout << endl << newpro.PID << " " << "Continued\t" << toString(newpro.args) << endl;
					} else if (WIFSIGNALED(pstatus)) {
						exitstatus = WEXITSTATUS(pstatus);
						 //int sig = WTERMSIG(pstatus);
						cout << endl << newpro.PID << " " << "Exited ("  << sigdesc << ")\t" << toString(newpro.args) << endl;
					   break;
					} // if
					//sleep(1);
				  } // while
				  
				} //if
				else {
					newpro.status = -100; //indicates that background process is running
				}
				if(bg_process)
					provec.push_back(newpro);
			} // end piping if pipe_count == 1
			
			/* FOR MULTIPLE PIPES */
			else if(pipe_count > 1)
			{
				int pipefd [2][2];
				int pid, pid2, pid3;
				bool bg_process = false;
				int rows = processes.size()-1;
				int cols = processes[rows].size()-1; 
				if(processes[rows][cols] == "&")
					bg_process = true;
				
				//first process
				if(pipe(pipefd[0]) == -1)
					perror("pipe");
				if((pid = fork()) == -1)
					perror("fork");
				if(pid != 0)
					newpro.PID = pid;
				else if(pid == 0)
				{
					if(bg_process)
						setpgid(0,0);
					
					if(dup2(pipefd[0][1], STDOUT_FILENO) == -1)
						perror("dup2(3)");
					
					//cerr << "STDOUT_FILENO (first): " << pipefd[0][1] << "**" << endl;
					
					close_pipe(pipefd[0]);
					
					int redirectindex = redirect(processes[0],processes[0].size());
					
					if (redirectindex == -1) {
						//cout << "WWW" << endl;
						dup2(stdinnum,STDIN_FILENO);
						dup2(stdoutnum,STDOUT_FILENO);
						exit(EXIT_FAILURE);
						//continue;
					}
					
					
					char ** argz = new char * [redirectindex + 1];
					for(int j = 0; j < redirectindex; j++)
						argz[j] = strdup(processes[0][j].c_str());
					argz[redirectindex] = nullptr;
					
					
					//cerr << "executing first: " << argz[0] << endl;
					execvp(argz[0], argz);
					perror("1730sh");
					
					for(int j = 0; j < (redirectindex + 1); j++)
					free(argz[j]);
					delete[] argz;
					exit(EXIT_FAILURE);		
				} // else if
				/* All processes between first and last process */
				for(int k = 1; k < pipe_count; k++)
				{
					int wpid, pstatus;
					if(pipe(pipefd[1]) == -1)
						perror("pipe");
					if((pid2 = fork()) == -1)
						perror("fork");
					if(pid2 != 0)
						newpro.PID = pid2; 
					if(pid2 == 0)
					{
						if(bg_process)
							setpgid(0,0);
						
						//cerr << "pipefd[0][0] = " << pipefd[0][0] << endl;
						//cerr << "pipefd[0][1] = " << pipefd[0][1] << endl;
						//cerr << "pipefd[1][0] = " << pipefd[1][0] << endl;
						//cerr << "pipefd[1][1] = " << pipefd[1][1] << endl;
						
						//if(k == 1)
						//{
							if(dup2(pipefd[0][0], STDIN_FILENO) == -1) // read end of first pipe [0][0]
								perror("dup2(1)");
							
							//cerr << "STDIN_FILENO: " << pipefd[0][0]  << "*" << endl;
							
							if(dup2(pipefd[1][1], STDOUT_FILENO) == -1) // write end of 2nd pipe [1][1]
								perror("dup2(2)");
							
							//cerr << "STDOUT_FILENO: " << pipefd[1][1] << "*" << endl;
						//}
						/*
						else
						{
							if(dup2(pipefd[0][0], STDIN_FILENO) == -1)
								perror("dup2");
							
							cerr << "STDIN_FILENO: " << pipefd[0][0]  << "*" << endl;
							
							if(dup2(pipefd[0][1], STDOUT_FILENO) == -1)
								perror("dup2");
							
							cerr << "STDOUT_FILENO: " << pipefd[0][1] << "*" << endl;
						}*/
						
						//if(k == 1)
						close_pipe(pipefd[1]);
						close_pipe(pipefd[0]);
						
						
						
						int redirectindex = redirect(processes[k],processes[k].size());
						
						if (redirectindex == -1) {
							//cout << "WWW" << endl;
							dup2(stdinnum,STDIN_FILENO);
							dup2(stdoutnum,STDOUT_FILENO);
							exit(EXIT_FAILURE);
						//	continue;
						}
					
						
						char ** argz = new char * [redirectindex + 1];
						for(int j = 0; j < redirectindex; j++)
						argz[j] = strdup(processes[k][j].c_str());
						argz[redirectindex] = nullptr;
						
						
						//cerr << "executing middle: " << argz[0] << endl;
						execvp(argz[0], argz);
						perror("1730sh");
						
						for(unsigned int j = 0; j < (processes[k].size() + 1); j++)
						free(argz[j]);
						delete[] argz;
						exit(EXIT_FAILURE);
					} // else 
					else
					{
						close_pipe(pipefd[0]);
						pipefd[0][0] = pipefd[1][0];
						pipefd[0][1] = pipefd[1][1];
						
					}
					while ((wpid = waitpid(pid2, &pstatus, WUNTRACED | WCONTINUED)) > 0) {
						if (WIFEXITED(pstatus)) {
							exitstatus = WEXITSTATUS(pstatus);
							//cout << wpid << " " << "Exited ("  << WEXITSTATUS(pstatus) << ")\t" << argv[0] << endl;
						   break;
						} else if (WIFSTOPPED(pstatus)) {
							//cout << endl << wpid << " " << "Stopped\t" << argv[0] << endl;
						  //kill(wpid, SIGCONT);
						  break;
						} else if (WIFCONTINUED(pstatus)) {
							//cout << endl << wpid << " " << "Continued\t" << argv[0] << endl;
						} else if (WIFSIGNALED(pstatus)) {
							exitstatus = WEXITSTATUS(pstatus);
							 //int sig = WTERMSIG(pstatus);
							//cout << endl << wpid << " " << "Exited ("  << sigdesc << ")\t" << argv[0] << endl;
						   break;
						} // if
						//sleep(1);
					 } // while
				}
				
				//close_pipe(pipefd[0]);

				/* last process */
				if((pid3 = fork()) == -1)
					perror("fork");
				if(pid3 != 0)
					newpro.PID = pid3;
				else if(pid3 == 0)
				{
					int redirectindex = redirect(processes[pipe_count],processes[pipe_count].size());
					
					if(bg_process)
					{
						setpgid(0,0);
						redirectindex--;
					}
					//cerr << "pipefd[0][0] = " << pipefd[0][0] << endl;
					//cerr << "pipefd[0][1] = " << pipefd[0][1] << endl;
					//cerr << "pipefd[1][0] = " << pipefd[1][0] << endl;
					//cerr << "pipefd[1][1] = " << pipefd[1][1] << endl;
						
					if(dup2(pipefd[0][0], STDIN_FILENO) == -1)
						perror("dup2(4)");
					
					//cerr << "STDIN_FILENO (last): " << pipefd[0][0] << "**" << endl;
					
					close_pipe(pipefd[0]);
					
					if (redirectindex == -1) {
						//cout << "WWW" << endl;
						dup2(stdinnum,STDIN_FILENO);
						dup2(stdoutnum,STDOUT_FILENO);
						exit(EXIT_FAILURE);
					//	continue;
					}
					
					char ** argz = new char * [redirectindex + 1];
					for(int j = 0; j < redirectindex; j++)
					argz[j] = strdup(processes[pipe_count][j].c_str());
					argz[redirectindex] = nullptr;
									
					 
					//cerr << "executing last: " << argz[0] << endl;				
					execvp(argz[0], argz);
					perror("1730sh");
					
					for(unsigned int j = 0; j < (processes[pipe_count].size() + 1); j++)
					free(argz[j]);
					delete[] argz;
					exit(EXIT_FAILURE);		
				} // else if
				
				close_pipe(pipefd[1]);
				//waitpid(pid, nullptr, 0);
				pid_t wpid;
				int pstatus;
			  while ((wpid = waitpid(pid, &pstatus, WUNTRACED | WCONTINUED)) > 0) {
				if (WIFEXITED(pstatus)) {
					exitstatus = WEXITSTATUS(pstatus);
					//cout << wpid << " " << "Exited ("  << WEXITSTATUS(pstatus) << ")\t" << argv[0] << endl;
				   break;
				} else if (WIFSTOPPED(pstatus)) {
					//cout << endl << wpid << " " << "Stopped\t" << argv[0] << endl;
				  //kill(wpid, SIGCONT);
				  break;
				} else if (WIFCONTINUED(pstatus)) {
					//cout << endl << wpid << " " << "Continued\t" << argv[0] << endl;
				} else if (WIFSIGNALED(pstatus)) {
					exitstatus = WEXITSTATUS(pstatus);
					 //int sig = WTERMSIG(pstatus);
					//cout << endl << wpid << " " << "Exited ("  << sigdesc << ")\t" << argv[0] << endl;
				   break;
				} // if
				//sleep(1);
			  } // while
			  
			  if(!bg_process)
			  {
				  while ((wpid = waitpid(pid3, &pstatus, WUNTRACED | WCONTINUED)) > 0) {
					  newpro.status = pstatus;
					if (WIFEXITED(pstatus)) {
						exitstatus = WEXITSTATUS(pstatus);
						cout << newpro.PID << " " << "Exited ("  << WEXITSTATUS(pstatus) << ")\t" << toString(newpro.args) << endl;
					   break;
					} else if (WIFSTOPPED(pstatus)) {
						cout << endl << newpro.PID << " " << "Stopped\t" << toString(newpro.args) << endl;
						provec.push_back(newpro);
					  //kill(wpid, SIGCONT);
					  break;
					} else if (WIFCONTINUED(pstatus)) {
						cout << endl << newpro.PID << " " << "Continued\t" << toString(newpro.args) << endl;
					} else if (WIFSIGNALED(pstatus)) {
						exitstatus = WEXITSTATUS(pstatus);
						 //int sig = WTERMSIG(pstatus);
						cout << endl << newpro.PID << " " << "Exited ("  << sigdesc << ")\t" << toString(newpro.args) << endl;
					   break;
					} // if
					//sleep(1);
				  } // while
				  
			  }//if
			  else {
				  newpro.status = -100;
			  }
			  if(bg_process)
				  provec.push_back(newpro);
			} // if pipe_count > 1
		}// if pipe_count > 0
		bool background = false;
		if(already_piped)
		{
			int redirectindex = -1;
			int redirectcount = 0;
			for (int i = 0; i < argc; i++) {
					if (argv[i] == "<" || argv[i] == ">" || argv[i] == ">>" || argv[i] == "e>" || argv[i] == "e>>") {
						if (redirectcount == 0) {
							redirectindex = i;
						}
						if (argv[i] == "<" && i < argc-1) {
							int n = open(argv[i+1].c_str(),O_RDWR);
							if (n == -1) {
								cout << argv[i+1] << " does not exist" << endl;
								return -1;
							}
							dup2(n,STDIN_FILENO);
							close(n);
						}
						else if (argv[i] == ">" && i < argc-1) {
							int n = open(argv[i+1].c_str(),O_WRONLY|O_CREAT,0666);
							if (n == -1) {
								//cout << "HI" << endl;
								perror("open");
								return -1;
							}
							dup2(n,STDOUT_FILENO);
							close(n);
						}
						else if (argv[i] == ">>" && i < argc-1) {
							int n;
							if( access(argv[i+1].c_str(), F_OK ) != -1 ) {
								n = open(argv[i+1].c_str(),O_WRONLY|O_APPEND,0666);
							} else {
								n = open(argv[i+1].c_str(),O_WRONLY|O_CREAT,0666);
							}
							if (n == -1) {
								perror("open");
								return -1;
							}
							dup2(n,STDOUT_FILENO);
							close(n);
						}
						else if (argv[i] == "e>>" && i < argc-1){
							int n;
							if( access(argv[i+1].c_str(), F_OK ) != -1 ) {
								n = open(argv[i+1].c_str(),O_WRONLY|O_APPEND,0666);
							} else {
								n = open(argv[i+1].c_str(),O_WRONLY|O_CREAT,0666);
							}
							if (n == -1) {
								//cout << "HI" << endl;
								perror("open");
								return -1;
							}
							dup2(n,STDERR_FILENO);
							close(n);
						}
						else if (argv[i] == "e>" && i < argc-1) {
							int n = open(argv[i+1].c_str(),O_WRONLY|O_CREAT,0666);
							if (n == -1) {
								perror("open");
								return -1;
							}
							dup2(n,STDERR_FILENO);
							close(n);
						}
						redirectcount++;
					}
			}
			if (redirectindex == -1) {
				redirectindex = argc;
			}
			if (redirectindex == -2) {
				dup2(stdinnum,STDIN_FILENO);
				dup2(stdoutnum,STDOUT_FILENO);
				dup2(stderrnum,STDERR_FILENO);
				continue;
			}
			//bool background = false;
			if (argv[redirectindex-1] == "&") {
				background = true;
				redirectindex--;
			}
			/*char ** c_string = new char*[argc + 1]; //increased size by one to add nullptr
			for (int r = 0; r < argc; r++) {
				c_string[r] = new char[argv[r].length()];
			}
			for (int r = 0; r < argc; r++) {
				for (unsigned int index = 0; index < argv[r].length(); index++){
					c_string[r][index] = argv[r][index];
				}
			}
			c_string[argc] = nullptr; //added nullptr into array*/
			
			char ** c_string = new char*[redirectindex + 1]; //increased size by one to add nullptr
			for (int r = 0; r < redirectindex; r++) {
				c_string[r] = new char[argv[r].length() + 1];
			}
			for (int r = 0; r < redirectindex; r++) {
				for (unsigned int index = 0; index < argv[r].length(); index++){
					c_string[r][index] = argv[r][index];
				}
				c_string[r][argv[r].length()] = '\0';
			}
			c_string[redirectindex] = nullptr; //added nullptr into array
			/*
			for(int i = 0; i < argc; i++)
				cout << "c_string[" << i << "]: " << c_string[i] << endl;
			*/
			
			  pid_t pid, wpid;                   // various PIDs
			  int pstatus;                       // process pstatus

			  int p = 0;
			   
			  pid = fork();
			  //setpgid(pid,0);
			  if (pid != 0) {
				 
				  newpro.PID = pid;
				 /* tcsetpgrp(STDIN_FILENO, pid);
				  tcsetpgrp(STDOUT_FILENO, pid);
				  tcsetpgrp(STDERR_FILENO, pid);*/
			  }
			//  newpro.PID = pid;
			  if (pid != 0) {
				 // cout << "HI" << endl;
				//setpgid(pid,0);
				if (background) {
				 newpro.PID = pid;
				}
				
			  }
			  if ((pid) < 0) {          // error 
				perror("fork");
			  } 
			  else if (pid == 0) {             // in child
				//cout << "HI" << endl;
				if (background) {
					setpgid(0,0);
				}
				else {
					//newpro.PID = pid;
				}
				//signal(SIGINT,child_handler);
				signal(SIGINT,SIG_DFL);
				signal(SIGQUIT,SIG_DFL);
				signal(SIGKILL,SIG_DFL);
				signal(SIGTSTP,SIG_DFL);
				//if (!background) {
				signal(SIGTTIN,SIG_DFL);
				signal(SIGTTOU,SIG_DFL);
				//
				signal(SIGTTIN,SIG_DFL);
				signal(SIGCHLD,SIG_DFL);
				//cout << endl << "Hello: " << c_string[0] << endl;
				p = execvp(c_string[0],c_string);
				//cout << "SICK" << endl;
				if (p == -1) {
					perror("1730sh");
				}
				exit(EXIT_SUCCESS);
			  } // if
			 // wait(0);
			  if (!background) {
				//cout << "BACKGROUND" << endl;
				while ((wpid = waitpid(pid, &pstatus, /*WNOHANG |*/ WUNTRACED | WCONTINUED)) > 0) 
				  {
					//cout << "gggg" << endl;
					if (WIFEXITED(pstatus)) {
						exitstatus = WEXITSTATUS(pstatus);
					  cout << newpro.PID << " " << "Exited ("  << WEXITSTATUS(pstatus) << ")\t" << argv[0] << endl;
					   break;
					} else if (WIFSTOPPED(pstatus)) {
						newpro.status = pstatus;
						provec.push_back(newpro);
					  cout << endl << newpro.PID << " " << "Stopped\t" << argv[0] << endl;
					  //kill(wpid, SIGCONT);
					  break;
					} else if (WIFCONTINUED(pstatus)) {
					  cout << endl << newpro.PID << " " << "Continued\t" << argv[0] << endl;
					} else if (WIFSIGNALED(pstatus)) {
						exitstatus = WEXITSTATUS(pstatus);
						 //int sig = WTERMSIG(pstatus);
						cout << endl << newpro.PID << " " << "Exited ("  << sigdesc << ")\t" << argv[0] << endl;
					   break;
					} // if
					//sleep(1);
				  } // while
				  newpro.status = pstatus;
				 
			  }
			  else {
				  newpro.status = -100; //set to running
			  }
			  for(int j = 0; j < (redirectindex); j++)
						free(c_string[j]);
			  delete[] c_string;
		}
		if (background) {
			provec.push_back(newpro);
		}
		dup2(stdinnum,STDIN_FILENO);
		dup2(stdoutnum,STDOUT_FILENO);
		dup2(stderrnum,STDERR_FILENO);
		// background = false;
		pgidcount++;
	} 
	//cout << "HI" << endl;
}



void close_pipe(int pipefd [2]) 
{
  if (close(pipefd[0]) == -1) 
  {
    perror("close");
    exit(EXIT_FAILURE);
  } // if
  if (close(pipefd[1]) == -1) 
  {
    perror("close");
    exit(EXIT_FAILURE);
  } // if
} // close_pipe

/*
while loop at begining of main loop using waitpid with -1 to check for all children, nohang
do this while waitpid returns > 0.
*/