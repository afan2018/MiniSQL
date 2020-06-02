#include <vector>
#include <cmath>
#include <iostream>
#include "../BufferManager/BufferManager.hpp"
#define TEST_N 3
#define NOT_VALID -1
using std::cin;
using std::cout;
using std::string;

#define DEBUG

int getEmptyBlockID() {

}

void removeBlock(int blockID) {

}

template <class T>
class BPTreeNode {
private:

    std::string fileName;
    int blockID;
    
    int dirtyLevel;
    bool blockRemoved;

    int keyLen;

    int parent;
    // int size;
    #ifndef DEBUG
    std::vector <T> keys;
    std::vector <int> records;
    std::vector <BPTreeNode*> children;
    #endif
    int rightNode;
    int leftNode;
    bool isLeaf;
    int N;
    BPTreeNode<T>* splitLeaf();
    BPTreeNode<T>* splitNonLeaf();

public:

    template <class U>
    friend void deleteNonLeaf(int pos, BPTreeNode<U>* node);

    BPTreeNode(string _fileName, int _blockID, int _keyLen, int _dirtyLevel);
    BPTreeNode(string _fileName, int _blockID, int _keyLen, int _parent, int _N, bool _isLeaf);
    ~BPTreeNode();

    #ifdef DEBUG
    std::vector <T> keys;
    std::vector <int> records;
    std::vector <int> children;
    #endif

    bool isRoot();
    bool isLeafNode();
    int getParent();
    bool isOverLoad(int size = -1);
    bool isHungry(int size = -1);
    int keyNum();

    int findRecordWithKey(const T& key);
    bool addKeyIntoNode(const T& key, int recordID);
    bool deleteKeyFromNode(const T& key);
    int getChildWithKey(const T& key);

    void pushKeyAndRecord(const T& key, int recordID);
    BPTreeNode<T>* split();

    bool tryReplaceKey(const T& oldKey, const T& newKey);
    void inflateLeaf();
};

template <class T>
BPTreeNode<T>::BPTreeNode(string _fileName, int _blockID, int _keyLen, int _dirtyLevel) : dirtyLevel(_dirtyLevel), fileName(_fileName), blockID(_blockID), keyLen(_keyLen) {
    int page = buf_mgr.getPageId(fileName + ".data", blockID);
    buf_mgr.pinPage(page);
    char* data = buf_mgr.getPageAddress(page);
    parent = *(reinterpret_cast<int*>(data));
    leftNode = *(reinterpret_cast<int*>(data + 4));
    rightNode = *(reinterpret_cast<int*>(data + 8));
    int size = *(reinterpret_cast<int*>(data + 12));
    children.push_back(*(reinterpret_cast<int*>(data + 16)));
    isLeaf = children[0] < 0;
    blockRemoved = false;

    if (dirtyLevel == 2) {
        int pos = 20;
        auto& copyto = isLeaf ? records : children;
        for (int i = 0; i < size; i++) {
            keys.push_back(*reinterpret_cast<T*>(pos));
            copyto.push_back(*reinterpret_cast<int*>(pos + keyLen));
            pos += keyLen + 4;
        }
    }

    buf_mgr.unpinPage(page);
}

template <class T>
BPTreeNode<T>::BPTreeNode(string _fileName, int _blockID, int _keyLen, int _parent, int _N, bool _isLeaf) : fileName(_fileName), blockID(_blockID), keyLen(_keyLen), parent(_parent), N(_N), isLeaf(_isLeaf) {
    if (isLeaf) children.push_back(-1);
    leftNode = rightNode = -1;
    dirtyLevel = 2;
    blockRemoved = false;
}

template <class T>
BPTreeNode<T>::~BPTreeNode() {
    if (dirtyLevel != 0 && !blockRemoved) {
        int page = buf_mgr.getPageId(fileName, blockID);
        char* data = buf_mgr.getPageAddress(page);
        int size = keys.size();
        memcpy(data, &parent, 4);
        memcpy(data + 4, &leftNode, 4);
        memcpy(data + 8, &rightNode, 4);
        memcpy(data + 12, &size, 4);
        memcpy(data + 16, &children[0], 4);
        if (dirtyLevel == 2) {
            int pos = 20;
            auto& copyfrom = isLeaf ? records : children;
            for (int i = 0; i < size; i++) {
                memcpy(data + pos, &keys[i], keyLen);
                memcpy(data + pos + keyLen, &copyfrom[isLeaf ? i : (i + 1)], 4);
                pos += keyLen + 4;
            }
        }
        buf_mgr.dirtPage(page);
        buf_mgr.unpinPage(page);
    }
}

template <class T>
inline int BPTreeNode<T>::keyNum() {
    return keys.size();
}

template <class T>
inline bool BPTreeNode<T>::isRoot() {
    return parent == -1;
}

template <class T>
inline bool BPTreeNode<T>::isLeafNode() {
    return isLeaf;
}

template <class T>
inline bool BPTreeNode<T>::isOverLoad(int size) {
    if (size == -1) size = keys.size();
    // return isLeaf ? size > N : size > N - 1;
    return size > N - 1;
}

template <class T>
inline bool BPTreeNode<T>::isHungry(int size) {
    if (parent == NULL) return false;
    return isLeaf ? size < std::ceil((N - 1) / 2.0) : size < std::ceil(N / 2.0) - 1;
}

template <class T>
inline int BPTreeNode<T>::getParent() {
    return parent;
}

template <class T>
int BPTreeNode<T>::findRecordWithKey(const T& key) {
    auto lb = std::lower_bound(keys.begin(), keys.end(), key);
    if (lb != keys.end() && *lb == key) return *lb;
    return NOT_VALID;
}

template <class T>
bool BPTreeNode<T>::addKeyIntoNode(const T& key, int recordID) {
    auto lb = std::lower_bound(keys.begin(), keys.end(), key);
    if (lb != keys.end() && *lb == key) return false;
    records.insert(lb - keys.begin() + records.begin(), recordID);
    keys.insert(lb, key);
    dirtyLevel = 2;
    return true;
}

// take care of type
template <class T>
bool BPTreeNode<T>::deleteKeyFromNode(const T& key) {
    auto lb = std::lower_bound(keys.begin(), keys.end(), key);
    if (lb != keys.end() && *lb == key) {
        records.erase(lb - keys.begin() + records.begin());
        keys.erase(lb);
        dirtyLevel = 2;
        return true;
    }
    return false;
}

template <class T>
int BPTreeNode<T>::getChildWithKey(const T& key) {
    if (isLeaf) return -1;
    auto lb = std::lower_bound(keys.begin(), keys.end(), key);
    if (lb == keys.end()) return children.back();
    return (key < *lb) ? children[lb - keys.begin()] : children[lb - keys.begin() + 1];
}

template <class T>
void BPTreeNode<T>::pushKeyAndRecord(const T& key, int recordID) {
    keys.push_back(key);
    records.push_back(recordID);
    dirtyLevel = 2;
}

// send ceil((n-1)/2) up to the parent and pointers from (ceil((n-1)/2) + 1) to ... send to splitNode
template <class T>
BPTreeNode<T>* BPTreeNode<T>::splitNonLeaf() {
    const auto upIndex = std::ceil((N - 1.0) / 2);
    const auto keyLen = keys.size();
    const auto upKey = keys[upIndex];
    // cout << upIndex;
    BPTreeNode<T>* splitNode = new BPTreeNode<T>(fileName, getEmptyBlockID(), keyLen, parent, N, isLeaf); 
    for (int i = upIndex + 1; i < keyLen; i++) {
        splitNode -> keys.push_back(keys[upIndex + 1]);
        splitNode -> children.push_back(children[upIndex + 1]);
        BPTreeNode<T>* child = new BPTreeNode<T>(fileName, children[upIndex + 1], keyLen, 1);
        child -> parent = splitNode;
        delete child;
        keys.erase(upIndex + 1 + keys.begin());
        children.erase(upIndex + 1 + children.begin());
    }
    splitNode -> children.push_back(children[upIndex + 1]);
    BPTreeNode<T>* child = new BPTreeNode<T>(fileName, children[upIndex + 1], keyLen, 1);
    child -> parent = splitNode;
    delete child;
    children.erase(upIndex + 1 + children.begin());
    keys.erase(upIndex + keys.begin());

    splitNode -> rightNode = rightNode;
    splitNode -> leftNode = blockID;
    if (rightNode != -1) {
        BPTreeNode<T>* rightPtr = new BPTreeNode<T>(fileName, rightNode, keyLen, 1);
        rightPtr -> leftNode = splitNode -> blockID;
        delete rightPtr;
    }
    rightNode = splitNode -> blockID;

    if (parent == -1) {
        BPTreeNode<T>* newRoot = new BPTreeNode<T>(fileName, getEmptyBlockID(), keyLen, -1, N, false);
        newRoot -> keys.push_back(upKey);
        newRoot -> children.push_back(blockID);
        newRoot -> children.push_back(splitNode -> blockID);
        parent = newRoot -> blockID;
        splitNode -> parent = newRoot -> blockID;
        delete splitNode;
        return newRoot;
    }

    BPTreeNode<T>* parentPtr = new BPTreeNode<T>(fileName, parent, keyLen, 2);
    auto& parentKeys = parentPtr -> keys;
    auto& parentChild = parentPtr -> children;
    auto it = std::lower_bound(parentKeys.begin(), parentKeys.end(), upKey);
    parentChild.insert(it - parentKeys.begin() + parentChild.begin() + 1, splitNode -> blockID);
    parentKeys.insert(it, upKey);
    parentPtr -> dirtyLevel = 2;
    delete splitNode;
    return parent;
}

// split overload node and insert smallest key of the new node into parent
template <class T>
BPTreeNode<T>* BPTreeNode<T>::splitLeaf() {
    BPTreeNode<T>* splitNode = new BPTreeNode<T>(fileName, getEmptyBlockID(), keyLen, parent, N, isLeaf); 
    // BPTreeNode<T>* splitNode = new BPTreeNode<T>(parent, N, isLeaf);
    const int len = keys.size();
    const int leftLen = std::ceil(len / 2.0);
    for (int i = leftLen; i < len; i++) {
        splitNode -> pushKeyAndRecord(keys[leftLen], records[leftLen]);
        keys.erase(keys.begin() + leftLen);
        records.erase(records.begin() + leftLen);
    }

    splitNode -> rightNode = rightNode;
    splitNode -> leftNode = blockID;
    if (rightNode != -1) {
        BPTreeNode<T>* rightPtr = new BPTreeNode<T>(fileName, rightNode, keyLen, 1);
        rightPtr -> leftNode = splitNode -> blockID;
        delete rightPtr;
    }
    rightNode = splitNode -> blockID;

    if (parent == -1) {
        // BPTreeNode<T>* newRoot = new BPTreeNode<T>(NULL, N, false);
        BPTreeNode<T>* newRoot = new BPTreeNode<T>(fileName, getEmptyBlockID(), keyLen, NULL, N, false); 
        newRoot -> pushKeyAndRecord(splitNode -> keys[0], splitNode -> records[0]);
        newRoot -> children.push_back(blockID);
        newRoot -> children.push_back(splitNode -> blockID);
        parent = newRoot -> blockID;
        splitNode -> parent = newRoot -> blockID;
        int ret = newRoot -> blockID;
        delete splitNode;
        return newRoot;
    }
    BPTreeNode<T>* parentPtr = new BPTreeNode<T>(fileName, parent, keyLen, 2);
    auto& parentKeys = parentPtr -> keys;
    // auto& parentRecord = parentPtr -> records;
    auto& parentChild = parentPtr -> children;
    auto it = std::lower_bound(parentKeys.begin(), parentKeys.end(), splitNode -> keys[0]);
    if (it == parentKeys.end()) {
        // parentRecord.push_back(splitNode -> records[0]);
        parentChild.push_back(splitNode -> blockID);
        parentKeys.push_back(splitNode -> keys[0]);
        int ret = parentPtr -> blockID;
        delete splitNode;
        return parent;
    }
    // parentRecord.insert(it - parentKeys.begin() + parentRecord.begin(), splitNode -> records[0]);
    parentChild.insert(it - parentKeys.begin() + parentChild.begin() + 1, splitNode -> blockID);
    parentKeys.insert(it, splitNode -> keys[0]);
    delete splitNode;
    return parent;
}

template <class T>
BPTreeNode<T>* BPTreeNode<T>::split() {
    return isLeaf ? splitLeaf() : splitNonLeaf();
}

template <class T>
void deleteNonLeaf(int pos, BPTreeNode<T>* node) {
    auto& nodeKeys = node -> keys;
    auto& nodeChild = node -> children;
    nodeKeys.erase(nodeKeys.begin() + pos);
    removeBlock(nodeChild[pos + 1]);
    nodeChild.erase(nodeChild.begin() + pos + 1);
    BPTreeNode<T>* parentPtr = new BPTreeNode<T>(node -> fileName, node -> parent, node -> keyLen, 2);
    if (node -> getParent() == -1 || (node -> keyNum() >= std::floor((node -> N + 1.0) / 2) - 1)) {
        return;
    }
    BPTreeNode<T>* leftPtr = NULL;
    BPTreeNode<T>* rightPtr = NULL;
    if (node -> leftNode != -1) leftPtr = new BPTreeNode<T>(node -> fileName, node -> leftNode, node -> keyLen, 2);
    if (node -> rightNode != -1) rightPtr = new BPTreeNode<T>(node -> fileName, node -> rightNode, node -> keyLen, 2);
    if (node -> leftNode != -1 && leftPtr -> parent == node -> parent && leftPtr -> keyNum() >= std::floor((node -> N + 1.0) / 2)) {
        int pos;
        int prtChildrenLen = parentPtr -> children.size();
        for (pos = 0; pos < prtChildrenLen; pos++) {
            if (parentPtr -> children[pos] == node -> leftNode) {
                break;
            }
        }
        nodeKeys.insert(nodeKeys.begin(), parentPtr -> keys[pos]);
        parentPtr -> keys[pos] = leftPtr -> keys.back();
        leftPtr -> keys.pop_back();
        nodeChild.insert(nodeChild.begin(), leftPtr -> children.back());
        leftPtr -> children.pop_back();
        nodeChild[0] -> parent = node -> blockID;
        delete leftPtr;
        delete rightPtr;
        delete node;
    } else if (node -> rightNode != -1 && rightPtr -> parent == node -> parent && rightPtr -> keyNum() >= std::floor((node -> N + 1.0) / 2)) {
        int pos;
        int prtChildrenLen = parentPtr -> children.size();
        for (pos = 0; pos < prtChildrenLen; pos++) {
            if (parentPtr -> children[pos] == node -> blockID) {
                break;
            }
        }
        nodeKeys.push_back(parentPtr -> keys[pos]);
        parentPtr -> keys[pos] = rightPtr -> keys.front();
        rightPtr -> keys.erase(rightPtr -> keys.begin());
        nodeChild.push_back(rightPtr -> children.front());
        nodeChild.back() -> parent = node -> blockID;
        rightPtr -> children.erase(rightPtr -> children.begin());
        delete leftPtr;
        delete rightPtr;
        delete node;
    } else if (node -> leftNode != -1 && leftPtr -> parent == node -> parent) {
        int pos;
        int prtChildrenLen = parentPtr -> children.size();
        for (pos = 0; pos < prtChildrenLen; pos++) {
            if (parentPtr -> children[pos] == node -> leftNode) {
                break;
            }
        }
        leftPtr -> keys.push_back(parentPtr -> keys[pos]);
        int len = nodeKeys.size();
        for (int i = 0; i < len; i++) {
            leftPtr -> keys.push_back(node -> keys.front());
            BPTreeNode<T>* child = new BPTreeNode<T>(node -> fileName, node -> children.front(), node -> keyLen, 1);
            child -> parent = node -> leftNode;
            delete child;
            leftPtr -> children.push_back(node -> children.front());
            node -> keys.erase(node -> keys.begin());
            node -> children.erase(node -> children.begin());
        }
        BPTreeNode<T>* child = new BPTreeNode<T>(node -> fileName, node -> children.front(), node -> keyLen, 1);
        child -> parent = node -> leftNode;
        delete child;
        leftPtr -> children.push_back(node -> children[0]);
        node -> children.erase(node -> children.begin());
        delete leftPtr;
        delete rightPtr;
        delete node;
        deleteNonLeaf<T>(pos, parentPtr);
    } else if (node -> rightNode != -1 && rightPtr -> parent == node -> parent) {
        int pos;
        int prtChildrenLen = parentPtr -> children.size();
        for (pos = 0; pos < prtChildrenLen; pos++) {
            if (parentPtr -> children[pos] == node -> blockID) {
                break;
            }
        }
        node -> keys.push_back(parentPtr -> keys[pos]);
        int len = rightPtr -> keys.size();
        for (int i = 0; i < len; i++) {
            node -> keys.push_back(rightPtr -> keys.front());
            node -> children.push_back(rightPtr -> children.front());
            BPTreeNode<T>* child = new BPTreeNode<T>(node -> fileName, node -> children.front(), node -> keyLen, 1);
            child -> parent = node -> blockID;
            delete child;
            rightPtr -> keys.erase(rightPtr -> keys.begin());
            rightPtr -> children.erase(rightPtr -> children.begin());
        }
        node -> children.push_back(rightPtr -> children.front());
        BPTreeNode<T>* child = new BPTreeNode<T>(node -> fileName, node -> children.back(), node -> keyLen, 1);
        child -> parent = node -> leftNode;
        delete child;
        node -> rightNode -> children.erase(node -> rightNode -> children.begin());
        deleteNonLeaf<T>(pos, parentPtr);
    }
}

template <class T>
void BPTreeNode<T>::inflateLeaf() {
    int thisSize = keys.size();
    BPTreeNode<T>* leftPtr = NULL;
    BPTreeNode<T>* rightPtr = NULL;
    BPTreeNode<T>* parentPtr = new BPTreeNode<T>(fileName, parent, keyLen, 2);
    if (leftNode != -1) leftPtr = new BPTreeNode<T>(fileName, leftNode, keyLen, 2);
    if (rightNode != -1) rightPtr = new BPTreeNode<T>(fileName, rightNode, keyLen, 2);
    if (leftNode != -1 && leftPtr -> parent == parent && leftPtr -> keyNum() >= floor((N + 1.0) / 2) + 1) {
        keys.insert(keys.begin(), leftPtr -> keys.back());
        records.insert(records.begin(), leftPtr -> records.back());
        leftPtr-> keys.pop_back();
        leftPtr -> records.pop_back();
        int pos;
        int prtChildrenLen = parentPtr -> children.size();
        for (pos = 0; pos < prtChildrenLen; pos++) {
            if (parentPtr -> children[pos] == leftNode) {
                break;
            }
        }
        parentPtr -> keys[pos] = keys.front();
    } else if (rightNode != -1 && rightPtr -> parent == parent && rightPtr -> keyNum() >= floor((N + 1.0) / 2) + 1) {
        keys.push_back(rightPtr -> keys.front());
        records.push_back(rightPtr -> records.front());
        rightPtr -> keys.erase(rightPtr -> keys.begin());
        rightPtr -> records.erase(rightPtr -> records.begin());
        int pos;
        for (pos = 0; pos < parentPtr -> children.size(); pos++) {
            if (parentPtr -> children[pos] == blockID) {
                break;
            }
        }
        parentPtr -> keys[pos] = rightPtr -> keys.front();
    } else if (leftNode != -1 && leftPtr -> parent == parent) { // BUG HERE!!!!!! sibling is not linked list
        for (int i = 0; i < thisSize; i++) {
            leftPtr -> keys.push_back(keys.front());
            leftPtr -> records.push_back(records.front());
            keys.erase(keys.begin());
            records.erase(records.begin());
        }
        leftPtr -> rightNode = rightNode;
        if (rightNode != -1) rightPtr -> leftNode = leftNode;
        int pos;
        int prtChildrenLen = parentPtr -> children.size();
        for (pos = 0; pos < prtChildrenLen; pos++) {
            if (parentPtr -> children[pos] == leftNode) {
                break;
            }
        }
        deleteNonLeaf<T>(pos, parent);
    } else if (rightNode != -1 && rightPtr -> parent == parent) {
        thisSize = rightPtr -> keyNum();
        for (int i = 0; i < thisSize; i++) {
            keys.push_back(rightPtr -> keys[i]);
            records.push_back(rightPtr -> records[i]);
            rightPtr -> keys.erase(rightPtr -> keys.begin());
            rightPtr -> records.erase(rightPtr -> records.begin());
        }
        if (rightPtr -> rightNode) {
            auto* rrPtr = new BPTreeNode<T>(fileName, rightPtr -> rightNode, keyLen, 1);
            rrPtr -> leftNode = blockID;
            delete rrPtr;
        }
        rightNode = rightPtr -> rightNode;
        int pos;
        int prtChildrenLen = parentPtr -> children.size();
        for (pos = 0; pos < prtChildrenLen; pos++) {
            if (parentPtr -> children[pos] == blockID) {
                break;
            }
        }
        deleteNonLeaf<T>(pos, parent);
    }
    // root and leaf
    delete parentPtr;
    delete leftPtr;
    delete rightPtr;
}

// B+ Tree
template <class T>
class BPTree {
    using BPNodePtr = BPTreeNode<T>*;
private:
    string fileName;
    int keyLen;
    int rtID;
    BPNodePtr findNodeWithKey(const T& key);       // returns b+ node pointer
    const int N;            

public:
    BPTree(string _fileName, int _keyLen, int _N);
    int root() {
        return rtID;
    }
    int findRecordWithKey(const T& key);                     // get the record for key
    bool insertRecordWithKey(const T& key, int recordID);    // insert key-record pair into b+
    bool deleteRecordWithKey(const T& key);                  // delete key-record pair from b+

    void print();                                            // debug
    void DFS(int xid, int d);
};

// empty tree
template <class T>
BPTree<T>::BPTree(string _fileName, int _keyLen, int _N) : fileName(_fileName), keyLen(_keyLen), N(_N) {
    BPNodePtr rootPtr = new BPTreeNode<T>(fileName, getEmptyBlockID(), keyLen, -1, N, true);
    rtID = rootPtr -> blockID;
    delete rootPtr;
};

template <class T>
BPTreeNode<T>* BPTree<T>::findNodeWithKey(const T& key) {
    int curNode = rtID;
    BPNodePtr curPtr = new BPTreeNode<T>(fileName, curNode, keyLen, 2);
    int nextNode = curPtr -> getChildWithKey(key);
    while(nextNode != -1) {
        curNode = nextNode;
        curPtr -> dirtyLevel = 0;
        delete curPtr;
        curPtr = new BPTreeNode<T>(fileName, curNode, keyLen, 2);
        nextNode = curPtr -> getChildWithKey(key);
    }
    curPtr -> dirtyLevel = 0;
    return curPtr;
}

template <class T>
int BPTree<T>::findRecordWithKey(const T& key) {
    BPNodePtr nodePtr = findNodeWithKey(key);
    if (!nodePtr) return NOT_VALID;
    int ret = nodePtr -> findRecordWithKey(key);
    nodePtr -> dirtyLevel = 0;
    delete nodePtr;
    return ret;
}

// take care that the overload/hungry check is handled by BPTree
template <class T>
bool BPTree<T>::insertRecordWithKey(const T& key, int recordID) {
    BPNodePtr toInsert = findNodeWithKey(key);
    if (!toInsert -> addKeyIntoNode(key, recordID)) {
        delete toInsert;
        return false;
    }
    BPNodePtr current = toInsert;
    while(current -> isOverLoad()) {
        BPNodePtr next = current -> split();
        delete current;
        current = next;
        if (current -> getParent() == NULL) {
            rtID = current -> blockID;
        }
    }
    return true;
}

template <class T>
bool BPTree<T>::deleteRecordWithKey(const T& key) {
    BPNodePtr toDelete = findNodeWithKey(key);
    if (!toDelete) return false;
    if (!toDelete -> deleteKeyFromNode(key)) {
        delete toDelete;
        return false;
    }
    if (toDelete -> keyNum() >= std::floor((N + 1.0) / 2)) {
        delete toDelete;
        return true;
    }
    toDelete -> inflateLeaf();
    BPNodePtr rootPtr = new BPTreeNode<T>(fileName, rtID, keyLen, 2);
    if (rootPtr -> children.size() == 1) {
        removeBlock(rtID);
        rtID = rootPtr -> children[0];
    }
    delete toDelete;
    delete rootPtr;
    return true;
}

// debug
template <class T>
void BPTree<T>::print() {
    DFS(rtID, 0);
}

template <class T>
void BPTree<T>::DFS(int xid, int d) {
    BPNodePtr x = new BPTreeNode<T>(fileName, xid, keyLen, 2);
    cout << "depth: " << d << " | ";
    for (auto el : x -> keys) {
        cout << el << ' ';
    }
    cout << "\n";
    for (auto id : x -> children) {
        DFS(id, d + 1);
    }
    if (x -> children.empty()) {
        cout << "(";
        for (auto rec : x -> records) {
            cout << rec << ", ";
        }
        cout << ")\n";
    }
    delete x;
}