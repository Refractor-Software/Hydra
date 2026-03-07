#include <foundation/foundation.h>
#include <foundation/containers/hash_map.h>

using namespace hydra::containers;

int main()
{
    hash_map<int, float> map;
    Append(map, 10, 30.f);
    return 0;
}
