#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <fstream>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sstream>
using namespace std;

#define SERVERIP "127.0.0.1"
#define INTERVAL 10
#define MAXCOST 10000
pthread_mutex_t mutex ; 

//helper functions and structs
struct item
{
	string dest;
	unsigned long long seq;
	double cost;
	double directcost;
	int port;
	string nexthop;
};

string lltostr(unsigned long long num)
{
	stringstream ss;
	ss<<num;
	string s;
	ss>>s;
	return s;
}
string doubletostr(double num)
{
	stringstream ss;
	ss<<num;
	string s;
	ss>>s;
	return s;
}
string inttostr(unsigned int num)
{
	stringstream ss;
	ss<<num;
	string s;
	ss>>s;
	return s;
}


class Node
{
private:
	string id;
	string datfile;
	int port;
public:
	//unsigned long long printednum;
	unsigned long long seq;
	vector<struct item> table;
	Node(int port1,string file);
	void ListenAndBroad();
	vector<struct item> get_table();
	string get_file();
	string get_id();
};

struct threadarg
{
	Node* node;
};

vector<struct item> Node::get_table()
{
	return table;
}

string Node::get_file()
{
	return datfile;
}

string Node::get_id()
{
	return id;
}

Node::Node(int port1,string file)
{
	//printednum=0;
	seq=0;
	port=port1;
	id="";
	datfile=file;

	ifstream fin;
	fin.open(file.c_str());
	if(!fin.is_open()){cout<<"Can not open file "<<file<<endl;return;}

	int linenum;
	fin>>linenum;
	fin>>id;
	for(int i=0;i<linenum;++i)
	{
		struct item item1;
		fin>>item1.dest;
		fin>>item1.cost;
		if(item1.cost<0)item1.cost=10000;
		fin>>item1.port;
		item1.seq=0;
		item1.nexthop=item1.dest;
		item1.directcost=item1.cost;
		table.push_back(item1);
		//cout<<item1.dest<<" "<<item1.cost<<" "<<item1.port<<endl;
	}
	fin.close();
}

void *broadcast_loop(void *ptr)
{
	struct threadarg* arg=(struct threadarg*)ptr;
	Node* node=arg->node;
	time_t timer=time(NULL);

	while(1)
	{
		if(time(NULL)-timer>=INTERVAL)
		{
			//check if datfile is changed
			ifstream fin;
			fin.open(node->get_file().c_str());
			if(!fin.is_open()){cout<<"broadcast_loop:open datfile error\n";return 0;}
			string uselessname;int num;
			vector<string>neighbors;vector<double>directcosts;vector<int>ports;
			fin>>num;fin>>uselessname;
			for(int i=0;i<num;++i)
			{
				string neighbor;double cost;int port;
				fin>>neighbor;fin>>cost;fin>>port;
				neighbors.push_back(neighbor);
				directcosts.push_back(cost);
				ports.push_back(port);
			}
			fin.close();


			//update data
			pthread_mutex_lock(&mutex);
			for(unsigned int i=0;i<node->table.size();++i)
			{
				for(unsigned int j=0;j<neighbors.size();++j)
				{
					if(neighbors[j]==node->table[i].dest)
					{
						if((directcosts[j]!=node->table[i].directcost)&&(!(directcosts[j]<0&&(node->table[i].directcost)==MAXCOST)))
						{
							double oldcost=node->table[i].cost;
							if(directcosts[j]<0)
							{node->table[i].directcost=MAXCOST;
							node->table[i].cost=MAXCOST;
							node->table[i].nexthop=neighbors[j];}
							else
							{node->table[i].directcost=directcosts[j];
							node->table[i].cost=directcosts[j];
							node->table[i].nexthop=neighbors[j];}
							node->table[i].seq+=1;
							for(unsigned int i1=0;i1<node->table.size();++i1)
							{
								if((node->table[i1].nexthop==neighbors[j])&&node->table[i1].dest!=neighbors[j])
								{
									//node->table[i1].cost=MAXCOST;
									if(directcosts[j]>=0&&(node->table[i1].cost)!=MAXCOST&&oldcost!=MAXCOST)
									{
										node->table[i1].cost+=(directcosts[j]-oldcost);
									}
									else node->table[i1].cost=MAXCOST;

									node->table[i1].seq+=1;
								}
							}
						}
					}
				}
			}


			//generate data to be sent
			string message="";
			message+=inttostr(node->table.size()+1);message.push_back(' ');
			message+=node->get_id();message.push_back('\n');
			cout<<"## print-out number "<<(node->seq)<<endl;
			for(unsigned int i=0;i<node->table.size();++i)
			{
				message+=lltostr(node->table[i].seq);message.push_back(' ');
				message+=node->table[i].dest;message.push_back(' ');
				message+=doubletostr(node->table[i].cost);message.push_back('\n');
				cout<<"shortest path to node "<<node->table[i].dest<<" (seq# "<<node->table[i].seq
					<<"): the next hop is "<<node->table[i].nexthop<<" and the cost is "<<node->table[i].cost
					<<", "<<(node->get_id())<<" -> "<<node->table[i].dest<<" : "<<node->table[i].cost<<endl;
			}
			message+=lltostr(node->seq);message.push_back(' ');
			message+=(node->get_id());message.push_back(' ');
			message+=doubletostr(0);message.push_back('\n');
			//node->printednum+=1;


			//send data to neighbors
			for(unsigned int i=0;i<ports.size();++i)
			{
				int sockfd;
				struct sockaddr_in servaddr;

				/* init servaddr */
				bzero(&servaddr, sizeof(servaddr));
				servaddr.sin_family = AF_INET;
				servaddr.sin_port = htons(ports[i]);
				if(inet_pton(AF_INET, SERVERIP, &servaddr.sin_addr) <= 0)
				{
					cout<<"broadcast_loop: bind IP error\n";
					return 0;
				}
				sockfd = socket(AF_INET, SOCK_DGRAM, 0);
				if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
				{
					cout<<"broadcast_loop: connect error\n";
					return 0;
				}
				if(directcosts[i]>=0)write(sockfd,message.c_str(),message.size());
			}
			//cout<<"Sent message:\n"<<message<<endl;
			timer=time(NULL);
			pthread_mutex_unlock(&mutex); 
		}
	}
	return 0;
}

void Node::ListenAndBroad()
{
	int sockfd;
	struct sockaddr_in servaddr, cliaddr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0); /* create a socket */

	/* init servaddr */
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	/* bind address and port to socket */
	if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
	{
		cout<<"Node::ListenAndBroad: bind error\n";
		return;
	}	

	//create a new thread to run broadcast_loop() and set lock
	pthread_mutex_init(&mutex,NULL);
	pthread_t id;
	struct threadarg *arg=(struct threadarg*)malloc(sizeof(struct threadarg));
	if(arg==NULL){cout<<"Node::ListenAndBroad: malloc error!" << endl;return;}
	arg->node=this;
	int ret = pthread_create(&id, NULL, broadcast_loop, arg);
	if(ret)
	{
		cout << "Node::ListenAndBroad: create pthread error!" << endl;
		pthread_mutex_destroy(&mutex);
 		return;
	}

	socklen_t len=sizeof(cliaddr);
	int recvnum=0;
	char buffer[1024];
	while(1)
	{
		recvnum=recvfrom(sockfd,buffer, 1024, 0,(struct sockaddr *)&cliaddr,&len);
		if(recvnum>0)
		{
			pthread_mutex_lock(&mutex);
			string message="";
			for(int i=0;i<recvnum;++i)message.push_back(buffer[i]);
			//cout<<"Received message:\n"<<message<<endl;
			stringstream ss;
			ss<<message;
			int messageline;
			string from;double costoffrom=MAXCOST;
			ss>>messageline;
			ss>>from;
			for(unsigned int i=0;i<table.size();++i)
			{
				if(table[i].dest==from)
				{
					costoffrom=table[i].directcost;
				}
			}
			for(int i=0;i<messageline;++i)
			{
				bool iffound=false;
				unsigned long long seq1;string dest;double cost;
				ss>>seq1;ss>>dest;ss>>cost;
				if(dest==this->id)
				{
					if(seq1>this->seq)
					{
						this->seq+=2;
					}
				}
				else
				{
					for(unsigned int j=0;j<table.size();++j)
					{
						if(table[j].dest==dest)
						{
							iffound=true;
							if(seq1>table[j].seq)
							{
								table[j].seq=seq1;
								if((costoffrom!=MAXCOST)&&(cost!=MAXCOST))table[j].cost=cost+costoffrom;
								else table[j].cost=MAXCOST;
								table[j].nexthop=from;
							}
							else if(seq1==table[j].seq&&table[j].cost>(cost+costoffrom))
							{
								table[j].cost=cost+costoffrom;
								table[j].nexthop=from;
							}
							break;
						}
					}
					if(!iffound)
					{
						struct item newitem;
						newitem.dest=dest;
						newitem.seq=seq1;
						if(cost!=MAXCOST&&costoffrom!=MAXCOST)newitem.cost=cost+costoffrom;
						else newitem.cost=MAXCOST;
						newitem.directcost=MAXCOST;
						newitem.port=-1;
						newitem.nexthop=from;
						table.push_back(newitem);
					}
				}
			}
			pthread_mutex_unlock(&mutex); 
		}
	}
	pthread_mutex_destroy(&mutex);
}



int main(int argc,char* argv[])
{
	int port=atoi(argv[1]);
	string file=argv[2];
	Node node(port,file);
	node.ListenAndBroad();
}
