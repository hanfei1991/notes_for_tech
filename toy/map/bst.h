#pragma once

#include<utility>
#include<type_traits>
#include<exception>
#include<iostream>

namespace toy {

struct TreeNodeBase {
    TreeNodeBase * left = nullptr;
    TreeNodeBase * right = nullptr;
    TreeNodeBase * parent = nullptr;
};

template<typename Value>
struct TreeNode : public TreeNodeBase {
    Value value;

    TreeNode(const Value & value_) : value(value_){}
};

class BinarySearchTreeHelper {
public:
    static TreeNodeBase * leftMostNode(TreeNodeBase * node) {
        while(node -> left != nullptr) {
            node = node -> left;
        }
        return node;
    }

    static TreeNodeBase * rightMostNode(TreeNodeBase * node) {
        while(node -> right != nullptr) {
            node = node -> right;
        }
        return node;
    }

    static TreeNodeBase * increment(TreeNodeBase * node) {
        if (node->right != nullptr) {
            return leftMostNode(node->right);
        } else {
            TreeNodeBase * parent = node -> parent;
            while(parent -> right == node) {
                node = parent;
                parent = node -> parent;
            }
            return parent;
        }
    }

    static TreeNodeBase * decrement(TreeNodeBase * node) {
        if (node->left != nullptr) {
            return rightMostNode(node->right);
        } else {
            TreeNodeBase * parent = node -> parent;
            while(parent -> left != parent) {
                return parent;
            }
        }
    }

};

template<typename Value, bool if_const>
class IteratorImpl {
    using ref = std::conditional_t<if_const, const Value &, Value &>;
    using ptr = std::conditional_t<if_const, const Value *, Value *>;

    using Self = IteratorImpl<Value, if_const>;
public:

    TreeNodeBase * node;

    IteratorImpl(TreeNodeBase * node_) :node(node_) {}

    ref operator *() const {
        return static_cast<TreeNode<Value> *>(node)->value;
    }

    ptr operator ->() const {
        return &(*(*this));
    }

    Self & operator ++() {
        if (node -> parent == node) {
            throw "the pointer is pointing to the end of tree.";
        }

        node = BinarySearchTreeHelper::increment(node);
        return *this;
    }

    Self operator ++(int) {
        Self old = *this;
        ++(*this);
        return old;
    }

    Self & operator --() {
        node = BinarySearchTreeHelper::decrement(node);
        return *this;
    }

    Self operator --(int) {
        Self old = *this;
        --(*this);
        return old;
    }

    bool operator == (const Self & rhs) {
        return node == rhs.node;
    }

    bool operator != (const Self & rhs) {
        return node != rhs.node;
    }

};

template<typename Key, typename Value>
class Select1ST {
public:
    const Key & operator ()(const Value & value) {
        return value.first;
    }
};

template<typename Key, typename Compare, typename Value, typename KeyOfValue = Select1ST<Key, Value>>
class BinarySearchTree
{
public:
    using iterator = IteratorImpl<Value, false>;
    using const_iterator = IteratorImpl<Value, true>;

private:
    TreeNodeBase header;

    using BasePtr = TreeNodeBase *;
    using NodePtr = TreeNode<Value> *;

    Compare compare_op;

    KeyOfValue key_of_value;

    BasePtr root() {
        return header.left;
    }

    int compare(const Key & lhs, const Key & rhs) {
        if(compare_op(lhs, rhs)) {
            return -1;
        } else if(compare_op(rhs, lhs)) {
            return 1;
        }
        return 0;
    }

    void insert_left(BasePtr parent, BasePtr child) {
        if (child != nullptr)
            child -> parent = parent;
        parent -> left = child;
    }

    void insert_right(BasePtr parent, BasePtr child) {
        if (child != nullptr)
            child -> parent = parent;
        parent -> right = child;
    }

    void eraseSingleNode(BasePtr node) {
        bool is_left_child = node->parent->left == node;
        BasePtr parent = node -> parent;
        if (node-> left == nullptr) {
            if (is_left_child)
                insert_left(parent, node -> right);
            else
                insert_right(parent, node -> right);
        } else if (node -> right == nullptr) {
            if (is_left_child)
                insert_left(parent, node -> left);
            else
                insert_right(parent, node -> left);
        } else {
            if (is_left_child)
                insert_left(parent, node -> right);
            else
                insert_right(parent, node -> right);
            BasePtr successor = BinarySearchTreeHelper::leftMostNode(node->right);
            insert_left(successor, node -> left);
        }

        delete static_cast<NodePtr>(node);

    }

    NodePtr findImpl(const Key & key) {
        if (nullptr == root()) {
            return nullptr;
        } else {
            NodePtr parent = static_cast<NodePtr>(root());
            for(;;) {
                auto result = compare(key, key_of_value(parent->value));
                if (result == 0) {
                    return parent;
                } else if (result == 1) {
                    // Insert value is larger than parent;
                    if (parent -> right == nullptr) {
                        return nullptr;
                    } else {
                        parent = static_cast<NodePtr>(parent -> right);
                    }
                } else {
                    // Insert value is smaller than parent;
                    if (parent -> left == nullptr) {
                        return nullptr;
                    } else {
                        parent = static_cast<NodePtr>(parent -> left);
                    }
                }
            }
        }
    }

public:

    BinarySearchTree() : header() {
        header.parent = & header;
    }

    std::pair<iterator, bool> insert_unique(const Value & value) {
        NodePtr node = new TreeNode<Value>(value);
        if (nullptr == root()) {
            insert_left(&header, node);
            return std::make_pair(iterator(node), true);
        } else {
            NodePtr parent = static_cast<NodePtr>(root());
            for(;;) {
                auto result = compare(key_of_value(node->value), key_of_value(parent->value));
                if (result == 0) {
                    return std::make_pair(iterator(parent), false);
                } else if (result == 1) {
                    // Insert value is larger than parent;
                    if (parent -> right == nullptr) {
                        insert_right(parent, node);
                        return std::make_pair(iterator(node), true);
                    } else {
                        parent = static_cast<NodePtr>(parent -> right);
                    }
                } else {
                    // Insert value is smaller than parent;
                    if (parent -> left == nullptr) {
                        insert_left(parent, node);
                        return std::make_pair(iterator(node), true);
                    } else {
                        parent = static_cast<NodePtr>(parent -> left);
                    }
                }
            }
        }
    }

    bool erase(const Key & key) {
        if (nullptr == root()) {
            return false;
        } else {
            NodePtr parent = static_cast<NodePtr>(root());
            for(;;) {
                auto result = compare(key, key_of_value(parent->value));
                if (result == 0) {
                    eraseSingleNode(parent);
                    return true;
                } else if (result == 1) {
                    // Insert value is larger than parent;
                    if (parent -> right == nullptr) {
                        return false;
                    } else {
                        parent = NodePtr(parent -> right);
                    }
                } else {
                    // Insert value is smaller than parent;
                    if (parent -> left == nullptr) {
                        return false;
                    } else {
                        parent = NodePtr(parent -> left);
                    }
                }
            }
        }
    }

    iterator find(const Key & key) {
        NodePtr node = findImpl(key);
        if (node == nullptr) {
            return end();
        }
        return iterator(node);
    }

    const_iterator find(const Key & key) const {
        NodePtr node = findImpl(key);
        if (node == nullptr) {
            return end();
        }
        return const_iterator(node);
    }

    iterator begin() {
        if(nullptr == root())
            return &header;
        return BinarySearchTreeHelper::leftMostNode(root());
    }

    const_iterator begin() const {
        if(nullptr == root())
            return const_iterator(&header);
        return root();
    }

    iterator end() {
        return iterator(&header);
    }

    const_iterator end() const {
        return const_iterator(&header);
    }
};

} // namespace toy
