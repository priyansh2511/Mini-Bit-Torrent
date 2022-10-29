#include <bits/stdc++.h>
using namespace std;
class Peer
{
public:
    string ip_address;
    string port;
    string user_name;
    int socket_fd;
    string listener_port;
};
typedef struct T
{
    Peer peer;
    vector<string> tokens;
} ThreadInfo;
void fun(Peer p, vector<string> x)
{
    ThreadInfo *info = new ThreadInfo;
    info->tokens = x;
    info->peer.ip_address = p.ip_address;
    cout << "Hello";
}
int main()
{
    Peer p = Peer();
    vector<string> x = {"sad", "mad"};
    p.ip_address = "23423";
    p.listener_port = "34534";
    fun(p, x);
    return 0;
}