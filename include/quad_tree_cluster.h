#include "cluster.h"
#include "Quadtree.h"
#include "Box.h"
template <typename data_type>
class quadtree_cluster
{
    class node
    {
        quadtree::Box<short> box;
    }
    class box_getter
    {
        operator() (node* node)
        {
            return node->box;
        }
    }
    cluster<data_type> cluster_;
    using bitmap = std::vector<bool>; 
    quadtree::Quadtree<node*, box_getter> quad_neighbors_;
    quadtree_cluster
    {
        auto root_box = quadtree::Box(0, 0, 256, 256);
        auto box_getter = box_getter();
        quad_neighbors_ = quadtree::Quadtree<Node*, box_getter>(root_box, box_getter);
        
    }

};