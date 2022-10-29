#include<bits/stdc++.h>
#include<pthread.h>
using namespace std;
string extract_file_name(string &path)
{
    string file_name=path.substr(path.find_last_of('/')+1,path.size()-path.find_last_of('/')+1);
    return file_name;
}
int main()
{
    string path;
    //cin>>path;
    int x=10;
    while(x--)
    {
        cout<<"Hello";
    }
    sleep(0.05);
    //cout<<extract_file_name(path);
    return 0;
}