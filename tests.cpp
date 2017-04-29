#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "objectpool.hpp"
#include "gmock/gmock.h"

using ::testing::AtLeast;

namespace {

    class EmptyObject {
    public:
        virtual ~EmptyObject() {};
        virtual void closeFunc() = 0;
        virtual void freeFunc() = 0;
    };

    class Object : public EmptyObject {
    public:
        MOCK_METHOD0(closeFunc, void());
        MOCK_METHOD0(freeFunc, void());
    };

    class PoolTest : public ::testing::Test {
    protected:

        PoolTest() {
        }

        virtual ~PoolTest() {
            if (m_obj != nullptr) {
                delete m_obj;
            }
        }

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }

        Object *m_obj = nullptr;
        void setObj(Object *obj) {
            if (m_obj != nullptr) {
                delete m_obj;
            }
        }
    };

    TEST_F(PoolTest, StartSize) {
        int count = 0;
        const int startSize = 3;

        ObjectPool<int> pool(
                [&count] () -> int* {
                    count++;
                    return nullptr;
                },
                [] (int *obj) {},
                [] (int *obj) {},
                startSize,
                5
            );

        ASSERT_EQ(count, startSize);
    }

    TEST_F(PoolTest, Destructor) {
        Object obj;
        const int n = 8;

        EXPECT_CALL(obj, freeFunc()).Times(n);
        EXPECT_CALL(obj, closeFunc()).Times(0);

        ObjectPool<Object> pool(
                [&obj] { return &obj; },
                [] (Object *obj) { obj->freeFunc(); },
                [] (Object *obj) {},
                n,
                n
        );

    }

    TEST_F(PoolTest, BackToPool) {
        Object obj;
        const int n = 8;

        EXPECT_CALL(obj, freeFunc()).Times(n);
        EXPECT_CALL(obj, closeFunc()).Times(n);

        ObjectPool<Object> pool(
                [&obj] { return &obj; },
                [] (Object *obj) { obj->freeFunc(); },
                [] (Object *obj) { obj->closeFunc(); },
                n,
                n
        );

        for (int i = 0; i < n; i++) {
            auto ptr = pool.object();
        }
    }

    TEST_F(PoolTest, NormalSize) {
        /*
        setObj(new Object());
        const int n = 8;

        EXPECT_CALL(*m_obj, freeFunc()).Times(n - 1);
        EXPECT_CALL(*m_obj, closeFunc()).Times(1);
        Object *obj = m_obj;

        auto *pool = new ObjectPool<Object>(
                [obj] { return obj; },
                [] (Object *obj) { obj->freeFunc(); },
                [] (Object *obj) { obj->closeFunc(); },
                1,
                1
        );

        auto *ptrs = new std::vector<ObjectPool<Object>::ObjectPtr>(n);

        for (int i = 0; i < n; i++) {
            ptrs->push_back(pool->object());
        }

        delete ptrs;
        //memmory leak*/
    }
}

int main(int argc, char **argv) {
    ::testing::GTEST_FLAG(throw_on_failure) = false;
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}