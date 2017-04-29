#ifndef OBJECTPOOL_HPP
#define OBJECTPOOL_HPP

#include <memory>
#include <functional>
#include <deque>
#include <mutex>
#include <atomic>

template <typename Object>
class ObjectPool
{
public:
    using ObjectPtr = std::unique_ptr<Object, std::function<void(Object*)>>;
    using ObjectFreeFunction = std::function<void(Object*)>;
    using ObjectBackToPoolFunction = std::function<void(Object*)>;
    using ObjectFabric = std::function<Object*()>;
    using Ptr = std::shared_ptr<ObjectPool<Object>>;

    explicit ObjectPool(ObjectFabric fabric, ObjectFreeFunction freeFunc, ObjectBackToPoolFunction backFunc, int startSize, int maxSize);
    ~ObjectPool();
    void destroy();

    ObjectPtr object();
    int count() const;

private:
    void freeObject(Object *object);

    std::deque<Object*> m_objs;
    ObjectBackToPoolFunction m_backFunc;
    ObjectFreeFunction m_freeFunc;
    ObjectFabric m_fabric;
    std::mutex m_ojbsMutex;
    int m_normalSize;
    bool m_shouldDestroy;
    std::atomic_int m_objsCount;
};

template <typename Object>
class ObjectPoolBuilder
{
public:

    ObjectPoolBuilder();

    ObjectPoolBuilder &setObjectBackToPoolFunction(typename ObjectPool<Object>::ObjectBackToPoolFunction connectionBackToPoolFunc);
    ObjectPoolBuilder &setObjectFreeFunction(typename ObjectPool<Object>::ObjectFreeFunction connectionFreeFunc);
    ObjectPoolBuilder &setObjectFabric(typename ObjectPool<Object>::ObjectFabric connectionFabric);
    ObjectPoolBuilder &setStartPoolSize(int size);
    ObjectPoolBuilder &setNormalPoolSize(int size);

    typename ObjectPool<Object>::Ptr build();

private:
    typename ObjectPool<Object>::ObjectBackToPoolFunction m_connectionBackToPoolFunc;
    typename ObjectPool<Object>::ObjectFreeFunction m_connectionFreeFunc;
    typename ObjectPool<Object>::ObjectFabric m_connectionFabric;
    int m_startPoolSize;
    int m_normalPoolSize;
};

template<typename Object>
typename ObjectPool<Object>::ObjectPtr ObjectPool<Object>::object()
{
    Object *con = nullptr;

    m_ojbsMutex.lock();
    if (!m_objs.empty())
    {
        con = m_objs.front();
        m_objs.pop_front();
    }
    m_ojbsMutex.unlock();

    if (!con)
    {
        con = m_fabric();
    }

    m_objsCount++;

    return ObjectPtr(con, [this](Object *ptr){ this->freeObject(ptr); });
}

template<typename Object>
ObjectPool<Object>::ObjectPool(
                                   ObjectPool::ObjectFabric fabric,
                                   ObjectPool::ObjectFreeFunction freeFunc,
                                   ObjectPool::ObjectBackToPoolFunction backFunc,
                                   int startSize,
                                   int normalSize
                               )
        : m_fabric(fabric)
        , m_freeFunc(freeFunc)
        , m_backFunc(backFunc)
        , m_normalSize(normalSize)
        , m_shouldDestroy(false)
        , m_objsCount(0)
{
    while (startSize)
    {
        m_objs.push_back(m_fabric());
        startSize--;
    }
}

template<typename Object>
void ObjectPool<Object>::destroy()
{
    m_ojbsMutex.lock();
    m_shouldDestroy = true;

    m_ojbsMutex.unlock();
}

template<typename Object>
void ObjectPool<Object>::freeObject(Object *object)
{
    m_ojbsMutex.lock();
    if (m_shouldDestroy || (m_objsCount > m_normalSize && m_normalSize > 0))
    {
        m_freeFunc(object);
    }
    else
    {
        m_backFunc(object);
        m_objs.push_back(object);
    }    
    m_objsCount--;

    if (m_shouldDestroy && m_objsCount == 0)
    {
        //should prevent double-free I hope
        m_objsCount++;
        m_ojbsMutex.unlock();

        delete this;
    }
    m_ojbsMutex.unlock();
}

template<typename Object>
int ObjectPool<Object>::count() const
{
    return m_objsCount;
}


template<typename Object>
ObjectPool<Object>::~ObjectPool()
{
    while (!m_objs.empty())
    {
        m_freeFunc(m_objs.front());
        m_objs.pop_front();
    }
}

template<typename Object>
ObjectPoolBuilder<Object> &ObjectPoolBuilder<Object>::setObjectBackToPoolFunction(
        typename ObjectPool<Object>::ObjectBackToPoolFunction connectionBackToPoolFunc)
{
    m_connectionBackToPoolFunc = connectionBackToPoolFunc;
    return *this;
}

template<typename Object>
ObjectPoolBuilder<Object> &ObjectPoolBuilder<Object>::setObjectFreeFunction(
        typename ObjectPool<Object>::ObjectFreeFunction connectionFreeFunc)
{
    m_connectionFreeFunc = connectionFreeFunc;
    return *this;
}

template<typename Object>
ObjectPoolBuilder<Object> &ObjectPoolBuilder<Object>::setObjectFabric(
        typename ObjectPool<Object>::ObjectFabric connectionFabric)
{
    m_connectionFabric = connectionFabric;
    return *this;
}

template<typename Object>
ObjectPoolBuilder<Object> &ObjectPoolBuilder<Object>::setStartPoolSize(int size)
{
    m_startPoolSize = size;
    return *this;
}

template<typename Object>
ObjectPoolBuilder<Object> &ObjectPoolBuilder<Object>::setNormalPoolSize(int size)
{
    m_normalPoolSize = size;
    return *this;
}

template<typename Object>
ObjectPoolBuilder<Object>::ObjectPoolBuilder()
    : m_normalPoolSize(-1)
    , m_startPoolSize(3)
{

}

template<typename Object>
typename ObjectPool<Object>::Ptr ObjectPoolBuilder<Object>::build()
{
    if (!m_connectionFabric)
    {
        m_connectionFabric = [] () { return new Object(); };
    }

    if (!m_connectionBackToPoolFunc)
    {
        m_connectionBackToPoolFunc = [] (Object*) {};
    }

    if (!m_connectionFreeFunc)
    {
        m_connectionFreeFunc = [] (Object *ptr) { delete ptr; };
    }

    if (!m_normalPoolSize)
    {
        m_normalPoolSize = m_startPoolSize;
    }

    ObjectPool<Object> *pool = new ObjectPool<Object>(
                    m_connectionFabric,
                    m_connectionFreeFunc,
                    m_connectionBackToPoolFunc,
                    m_startPoolSize,
                    m_normalPoolSize
                );

    return ObjectPool<Object>::Ptr(pool, [](ObjectPool<Object> *pool) { pool->destroy(); });
}

#endif // OBJECTPOOL_HPP
