#include <catch2/catch_test_macros.hpp>
#include "core/entities/TexturePool.h"
#include <thread>
#include <vector>

using namespace macroman;

TEST_CASE("TexturePool - Basic Acquisition", "[core][texture_pool]") {
    TexturePool pool;
    // We intentionally do NOT call initialize() because it requires a D3D device.
    // The pool logic (acquire/release) should work on the pool structure regardless of GPU resources.

    SECTION("Initially all textures are available") {
        REQUIRE(pool.getAvailableCount() == 3);
    }

    SECTION("Acquire reduces available count") {
        auto handle = pool.acquireForWrite(1);
        REQUIRE(handle != nullptr);
        REQUIRE(pool.getAvailableCount() == 2);
    }

    SECTION("Release restores available count") {
        {
            auto handle = pool.acquireForWrite(1);
            REQUIRE(pool.getAvailableCount() == 2);
            // Handle goes out of scope here
        }
        REQUIRE(pool.getAvailableCount() == 3);
    }
}

TEST_CASE("TexturePool - Starvation Logic", "[core][texture_pool]") {
    TexturePool pool;

    SECTION("Cannot acquire more than pool size") {
        auto h1 = pool.acquireForWrite(1);
        auto h2 = pool.acquireForWrite(2);
        auto h3 = pool.acquireForWrite(3);

        REQUIRE(pool.getAvailableCount() == 0);

        // Fourth acquire should fail
        auto h4 = pool.acquireForWrite(4);
        REQUIRE(h4 == nullptr);
        REQUIRE(pool.getStarvedCount() == 1);
    }
}

TEST_CASE("TexturePool - RAII Compliance (Trap 1)", "[core][texture_pool]") {
    TexturePool pool;

    SECTION("TextureDeleter releases texture back to pool") {
        Texture* rawPtr = nullptr;
        {
            auto handle = pool.acquireForWrite(1);
            REQUIRE(handle != nullptr);
            rawPtr = handle.get();
            REQUIRE(pool.getAvailableCount() == 2);
            
            // Verify refCount is 1 via manual check? 
            // We can't access Texture::refCount directly as it is internal details usually, 
            // but we can trust getAvailableCount().
        }
        // Handle destroyed, should be back in pool
        REQUIRE(pool.getAvailableCount() == 3);
        
        // Re-acquiring should be possible (might get same or different slot)
        auto handle2 = pool.acquireForWrite(2);
        REQUIRE(handle2 != nullptr);
    }
}
