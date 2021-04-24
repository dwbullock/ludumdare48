/*******************************************************************************************
*
*  Deeper and deeper:
*     Deep thoughts (jack handy, the game).
*     Galaxy -> Solar System -> Earth -> Human -> Cell -> Atom -> Craziness
*     Deep into the mind
*     Deep into the ocean.
*     Deep in a dungeon.
*     Deep conspiracy.
*     Deep learning.
*     Deep crust pizza.
*     Deep neural nets - you pick the weights.
*
********************************************************************************************/

#include <chrono>
#include <thread>
#include <sstream>
#include <vector>
#include <memory>
#include <assert.h>
#include "raylib.h"
#include "raymath.h"

class Thing
{
public:
    Thing(const Vector2& position = { 0 }) : position(position), display(true) {}
    virtual void doRender() {}
    void render() {
        if (display)
            doRender();
    }
    Vector2 position;
    bool    display;
};

class Circle : public Thing
{
public:
    Circle(const float radius, const Color& color, const Vector2& position = { 0 })
        : Thing(position)
        , radius(radius)
        , color(color) {}

    virtual void doRender() override
    {
        DrawCircleV(position, radius, color);
    }

    Color color;
    float radius;
};



class Text : public Thing
{
public:
    Text(const Color& color, const Vector2& position = { 0 }, const std::string str = "")
        : Thing(position)
        , text(str)
        , color(color) {}

    virtual void doRender() override
    {
        DrawText(text.c_str(), int(position.x), int(position.y), 20, color);
    }

    std::string text;
    Color color;
};


class BallCountText : public Text
{
public:
    typedef Text super;
    BallCountText(const Vector2& position = { 0 })
        : Text(GRAY, position)
        , count(0) {}

    virtual void doRender() override
    {
        std::stringstream ss;
        ss << "You caught " << count << (count == 1 ? " ball." : " balls.");
        this->text = ss.str();
        super::doRender();
    }

    int count;
};

class WildCircle : public Thing
{
public:
    WildCircle(const int startRadius, const Color& startColor, const int numCircles, const int circleDelta, const Vector2& position = { 0 })
        : Thing(position)
        , startRadius(startRadius)
        , startColor(startColor)
        , circleDelta(circleDelta)
        , started(false)
        , numCircles(numCircles)
        , ring(0)
    {
    }

    virtual void doRender() override
    {
        if (!started)
            return;
        for (int ii = rings.size()-1; ii >= 0; --ii) {
            const float radius = static_cast<float>(startRadius + ii * circleDelta);
            DrawCircleV(position, radius, rings[ii]);
        }
    }

    void start(const Vector2 position)
    {
        if (started)
            return;
        started = true;
        this->position = position;
        rings.push_back(startColor);
        ring++;
    }

    void next()
    {
        std::vector<Color> palette = {
            {242, 242, 48},
            {194, 242, 97},
            {145, 242, 145},
            {97, 242, 194},
            {48, 242, 242}
        };
        if (!started)
            return;
        if (rings.size() < (size_t)numCircles) {
            rings.push_back(WHITE);
        }
        /*if (ring == 0) {
            position = Vector2{ (float)GetRandomValue(startRadius, GetScreenWidth() - startRadius),
                                (float)GetRandomValue(startRadius, GetScreenHeight() - startRadius) };
        }*/
        Color targetColor = palette[ring % palette.size()];
        float val = (float)GetRandomValue(600, 1000) / 1000.0f;
        rings[ring] = Color{ static_cast<unsigned char>((float)targetColor.r * val),
                             static_cast<unsigned char>((float)targetColor.g * val),
                             static_cast<unsigned char>((float)targetColor.b * val),
                             255 };
        ring = (ring + 1) % numCircles;
    }

    Color startColor;
    int startRadius;
    int circleDelta;
    int numCircles;
    int ring;
    bool started;
    std::vector<Color> rings;
};

bool isVisible(const Color& col)
{
    return !(col.a == 0 ||
        (col.r == 0 && col.g == 0 && col.b == 0));
}

typedef struct SimSpacePosition {
    SimSpacePosition(float slice = 0.0f, float positionInSlice = 0.0f)
        : slice(slice)
        , positionInSlice(positionInSlice) {}

    float slice;
    float positionInSlice;
} SimSpacePosition; 

void playerCornersInSimSpace(
    float playerSlice, float playerPosition, float playerWidthInSliceDiv2, float playerHeightSliceDirDiv2,
    SimSpacePosition& p0, SimSpacePosition& p1, SimSpacePosition& p2, SimSpacePosition& p3)
{
    p0 = { playerSlice + playerHeightSliceDirDiv2, playerPosition - playerWidthInSliceDiv2 };
    p1 = { playerSlice + playerHeightSliceDirDiv2, playerPosition + playerWidthInSliceDiv2 };
    p2 = { playerSlice - playerHeightSliceDirDiv2, playerPosition + playerWidthInSliceDiv2 };
    p3 = { playerSlice - playerHeightSliceDirDiv2, playerPosition - playerWidthInSliceDiv2 };
}

class LevelGeometry : public Thing
{
public:
    LevelGeometry()
        : Thing()
        , playerPosition(static_cast<float>(sliceWidth + sliceWidth / 2 + sliceHeight))
        , playerColor({ 45,227,191,255 })
        , playerSlice(10.0f)
        , numSlices(3000)
    {
        geom.resize(numSlices);
        for (int ii = 0; ii < numSlices; ++ii) {
            geom[ii].resize(sliceSize);
            //sliceVal = (ii % 10 == 0) ? (((jj % 10) < 7) ? 0 : 255) : 0;
            for (int jj = 0; jj < sliceSize; ++jj) {
                unsigned char sliceVal = 0;
                if ((ii % 20 == 0) && (jj > 1) && (jj != (sliceSize - 2))) {
                //if ((ii % 20 == 0) && (jj%7 == 0)) {
                //if (ii % 20 == 0 && jj%2) {
                    sliceVal = 255;
                }
                geom[ii][jj] = sliceVal;
            }
        }
        updateWorldGeom();
    }

    void updateWorldGeom() {
        worldGeom.resize(geom.size() + 1);
        for (size_t ii = 0; ii < worldGeom.size(); ++ii) {
            std::vector<Vector3>& worldSlice = worldGeom[ii];
            worldSlice.resize(sliceSize + 1);
            for (size_t jj = 0; jj < worldSlice.size(); ++jj) {
                worldSlice[jj] = toWorld(static_cast<float>(ii), jj);
            }
        }
    }
    void loadLevelFromImage(const char fname[]) {
        Image levelImage = LoadImage(fname);
        Color* colors = LoadImageColors(levelImage);
        numSlices = levelImage.width;
        int height = std::max(levelImage.height, sliceSize);

        geom.resize(numSlices);
        for (int ii = 0; ii < numSlices; ++ii) {
            geom[ii].resize(sliceSize);
            for (int jj = 0; jj < sliceSize; ++jj) {
                unsigned char val = 0;
                if (jj < levelImage.height) {
                    if (isVisible(colors[numSlices * jj + ii])) {
                        val = 255;
                    }
                }
                geom[ii][jj] = val;
            }
        }
        updateWorldGeom();
        UnloadImageColors(colors);
        UnloadImage(levelImage);
    }
    virtual void doRender() override;

    bool collides(float testPlayerSlice, float testPlayerPosition) {
        auto testSinglePoint = [=](const SimSpacePosition& spp) -> bool {
            int intSlice = static_cast<int>(floorf(spp.slice));
            int intSlicePosition = static_cast<int>(floorf(spp.positionInSlice));
            if (intSlice >= 0 && intSlice < static_cast<int>(geom.size())) {
                const auto& slice = geom[intSlice];
                if (intSlicePosition >= 0 && intSlicePosition < static_cast<int>(slice.size())) {
                    if (slice[intSlicePosition])
                        return true;
                }
            }
            return false;
        };

        SimSpacePosition sp0, sp1, sp2, sp3;
        playerCornersInSimSpace(testPlayerSlice, testPlayerPosition, playerWidthInSliceDiv2, playerHeightSliceDirDiv2, sp0, sp1, sp2, sp3);


        return testSinglePoint(sp0) || testSinglePoint(sp1) || testSinglePoint(sp2) || testSinglePoint(sp3);;
    }
    // returns true if there was a collision
    bool incrPlayerPosition(float val) {
        const float modulo = static_cast<float>(2 * sliceWidth + 2 * sliceHeight);
        float newPlayerPosition = fmodf(playerPosition + modulo + val, modulo);
        if (collides(playerSlice, newPlayerPosition)) {
            return true;
        }
        playerPosition = newPlayerPosition;
        return false;
    }

    bool incrPlayerSlice(float val) {
        float newPlayerSlice = playerSlice + val;
        if (collides(newPlayerSlice, playerPosition)) {
            return true;
        }
        playerSlice = newPlayerSlice;
        return false;
    }

    int numSlices = 3000;
    const int sliceWidth = 80;
    const int sliceHeight = 60;
    const float playerHeightSliceDirDiv2 = 0.5f;
    const float playerWidthInSliceDiv2 = 0.5f;
    //const int sliceWidth = 80;
    //const int sliceHeight = 60;
    const int sliceSize = sliceWidth * 2 + sliceHeight * 2;
    Color playerColor;

    // grid space
    std::vector< std::vector<unsigned char> > geom;
    float playerSlice; // which slice the player is on
    float playerPosition; // position on slice

    // items in world space
    const float                         worldWidth = static_cast<float>(400);
    const float                         worldHeight = static_cast<float>(300);
    std::vector< std::vector<Vector3> > worldGeom; // position of top-left point of each geom pixel; includes extra row/element to bake end 



  private:

      // convert image slice/index into slice to a point; gives left-top most world point of element
      Vector3 toWorld(float slice, int index) {
          // ensure index in range
          int modulo = sliceSize;
          while (index < 0) {
              index += modulo; // yikes
          }
          index = index % modulo;

          const float fSliceWidth = static_cast<float>(sliceWidth);
          const float fSliceHeight = static_cast<float>(sliceHeight);
          if (index < sliceWidth) {
              float t1 = float(index) / fSliceWidth;
              return(Vector3{ worldWidth * t1, 0.0f, slice });
          }

          index -= sliceWidth;
          if (index < sliceHeight) {
              float t1 = float(index) / fSliceHeight;
              return(Vector3{ worldWidth, worldHeight * t1, slice });
          }

          index -= sliceHeight;
          if (index < sliceWidth) {
              float t1 = 1.0f - float(index) / fSliceWidth;
              return(Vector3{ worldWidth * t1, worldHeight, slice });
          }

          index -= sliceWidth;
          float t1 = 1.0f - float(index) / fSliceHeight;
          return(Vector3{ 0, worldHeight * t1, slice });
      }

      Vector3 toWorldFloat(float slice, float index) {
          float index1 = floorf(index);
          float t = index - index1;
          int index1Int = static_cast<int>(index1);

          Vector3 p0 = toWorld(slice, index1Int);
          Vector3 p1 = toWorld(slice, index1Int+1);
          return Vector3Lerp(p0, p1, t);
      }

      Vector2 toScreen(const Vector3& world, float sliceAtCenter, float slicesPerScreen, const Vector2& screenCenter) {
        const float sliceFloat = world.z;
        if (sliceFloat > sliceAtCenter) {
            return screenCenter;
        }
        float scale = (sliceAtCenter - sliceFloat) / slicesPerScreen;
        const Vector2 center = { worldWidth / 2, worldHeight / 2 };
        Vector2 pos = Vector2Subtract({ world.x, world.y }, center);
        pos = Vector2Divide(pos, center); // normalize to -1..1
        pos = Vector2Scale(pos, scale);
        pos = Vector2Multiply(pos, screenCenter);
        pos = Vector2Add(pos, screenCenter);
        return pos;
    }

};

void LevelGeometry::doRender()
{
    const float centerX = static_cast<float>(GetScreenWidth() / 2);
    const float centerY = static_cast<float>(GetScreenHeight() / 2);
    const Vector2 screenCenter = { centerX, centerY };

    const float slicesBeforePlayer = 15.0f;
    const float sliceAtCenter = playerSlice + slicesBeforePlayer;
    const float slicesPerScreen = 20.0f;
    const int numSlicesToIterate = static_cast<int>(slicesPerScreen) + 2;
    const int sliceAtCenterInt = static_cast<int>(sliceAtCenter);

    // draw grid
    for (int ii = 0; ii < numSlicesToIterate; ++ii) {
        int currSliceIndex = sliceAtCenterInt - ii;
        if (currSliceIndex >= 0 && currSliceIndex < (static_cast<int>(worldGeom.size()) - 1)) {
            assert(size_t(currSliceIndex) < worldGeom.size());
            const std::vector<Vector3>& sliceWorld = worldGeom[currSliceIndex];
            const std::vector<Vector3>& nextSliceWorld = worldGeom[currSliceIndex + 1];
            assert(sliceWorld.size() <= size_t(sliceSize + 1));
            const std::vector<unsigned char>& slice = geom[currSliceIndex];
            //       b11-p3--------p2-b12
            //        |\   \xxxxxx/   /|
            //        | a11-p0--p1-a12 |
            //        |  |          |  |
            //        | a21--------a22 |
            //        |/              \|
            //       b21--------------b22
            for (int jj = 0; jj < sliceSize; jj++) {
                if (!slice[jj]) continue;

                const Vector2 p0 = toScreen(sliceWorld[jj], sliceAtCenter, slicesPerScreen, screenCenter);
                const Vector2 p1 = toScreen(sliceWorld[jj + 1], sliceAtCenter, slicesPerScreen, screenCenter);
                const Vector2 p2 = toScreen(nextSliceWorld[jj + 1], sliceAtCenter, slicesPerScreen, screenCenter);
                const Vector2 p3 = toScreen(nextSliceWorld[jj], sliceAtCenter, slicesPerScreen, screenCenter);
                DrawTriangle(p2, p1, p0, RED);
                DrawTriangle(p3, p2, p0, RED);
            }
        }
    }

    // draw player
    {
        SimSpacePosition sp0, sp1, sp2, sp3;
        playerCornersInSimSpace(playerSlice, playerPosition, playerWidthInSliceDiv2, playerHeightSliceDirDiv2, sp0, sp1, sp2, sp3);
        const Vector3 player0World = toWorldFloat(sp0.slice, sp0.positionInSlice);
        const Vector3 player1World = toWorldFloat(sp1.slice, sp1.positionInSlice);
        const Vector3 player2World = toWorldFloat(sp2.slice, sp2.positionInSlice);
        const Vector3 player3World = toWorldFloat(sp3.slice, sp3.positionInSlice);

        const Vector2 p0 = toScreen(player0World, sliceAtCenter, slicesPerScreen, screenCenter);
        const Vector2 p1 = toScreen(player1World, sliceAtCenter, slicesPerScreen, screenCenter);
        const Vector2 p2 = toScreen(player2World, sliceAtCenter, slicesPerScreen, screenCenter);
        const Vector2 p3 = toScreen(player3World, sliceAtCenter, slicesPerScreen, screenCenter);
        DrawTriangle(p0, p1, p2, playerColor);
        DrawTriangle(p0, p2, p3, playerColor);
    }
}

class GameState
{
public:
    enum class PlayerDirection { CCW, CW, IN, OUT };

    GameState()
      : playerDirection(PlayerDirection::IN)
      , simTick(0)
    {
        lg.loadLevelFromImage("Content/test_level.png");
    }

    void Sim(float simTimeSeconds) {
        const float playerHorizontalSpeed = 0.75;
        const float playerVerticalSpeed = 0.5;

        if (IsKeyDown(KEY_UP)) playerDirection = PlayerDirection::IN;
        if (IsKeyDown(KEY_DOWN)) playerDirection = PlayerDirection::OUT;
        if (IsKeyDown(KEY_LEFT)) playerDirection = PlayerDirection::CCW;
        if (IsKeyDown(KEY_RIGHT)) playerDirection = PlayerDirection::CW;
        if (playerDirection == PlayerDirection::IN) {
            lg.incrPlayerSlice(playerVerticalSpeed);
        }
        else if (playerDirection == PlayerDirection::OUT) {
            lg.incrPlayerSlice(-playerVerticalSpeed);
        }
        else if (playerDirection == PlayerDirection::CCW) {
            lg.incrPlayerPosition(-playerHorizontalSpeed);
        }
        else if (playerDirection == PlayerDirection::CW) {
            lg.incrPlayerPosition(playerHorizontalSpeed);
        }
        simTick++;
    }
    void Render() {
        BeginDrawing();
        ClearBackground(BLACK);

        lg.render();

        EndDrawing();
    }

    int simTick;
    LevelGeometry lg;
    PlayerDirection playerDirection;
};





int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 600;
    const int fps = 60;
    float simTimeSeconds = 1.0f / static_cast<float>(fps);

    InitWindow(screenWidth, screenHeight, "Ludumdare 48");
    InitAudioDevice();
    SetTargetFPS(fps);

    // Main game loop
    GameState gameState;
    while (!IsAudioDeviceReady()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        gameState.Sim(simTimeSeconds);
        gameState.Render();
    }

    CloseWindow();        // Close window and OpenGL context
    CloseAudioDevice();
    return 0;
}


