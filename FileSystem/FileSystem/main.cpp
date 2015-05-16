#include "fileSystem.h"

void main()
{
	cout<<"FileSystem beta1.0"<<endl;
	cout<<"copy right @ ¶­ÅÊ"<<endl;
	cout<<"ID:201226100403"<<endl;
	cout<<"e-mail:linxury@foxmail.com"<<endl<<endl;
	loadSuper("vm.dat");
	root=iget(0);
	while(!login())
	{
		NULL;
	}
	current=root;
	while(!logout)
	{
		dispatcher();
	}
}