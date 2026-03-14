#include <Foundation/Foundation.h>
#include <Foundation/Containers/HashMap.h>

using namespace hydra::containers;

int main()
{
    hash_map<int, float> map;
    map.Append(30, 10.f);
    return 0;
}
