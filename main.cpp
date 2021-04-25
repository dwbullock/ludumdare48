/*******************************************************************************************
* (c) 2021 DancesWE
* 
* Sparks on player
* Phase in/out on energy zone
* Procedural maps
* Powerup for depth of view.
* Powerup for speed.
* 
* Music
* Intro screen
* Momentum for play
*
********************************************************************************************/

#include <chrono>
#include <thread>
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <assert.h>
#include "raylib.h"
#include "raymath.h"

int gSimTick = 0;

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

class SafeImage
{
    public:
        SafeImage(const SafeImage&) = delete;
        SafeImage& operator=(SafeImage const&) = delete;

        SafeImage(const char fname[]) {
            image = LoadImage(fname);
            colorArr = LoadImageColors(image);
        }
        ~SafeImage() {
            UnloadImageColors(colorArr);
            UnloadImage(image);
        }
        Color& color(int x, int y) const { return colorArr[image.width * y + x]; }
        Color* colorArr;
        Image image;
};

float RandFloat()
{
    return static_cast<float>(rand() % 1001) / 1000.0f;
}

float RandFloat(float max)
{
    return max * (static_cast<float>(rand() % 1001) / 1000.0f);
}

class LevelTransformer
{
public:
    LevelTransformer(int sliceSize, int sliceWidth, int sliceHeight, float worldWidth, float worldHeight)
        : sliceSize(sliceSize)
        , sliceWidth(sliceWidth)
        , sliceHeight(sliceHeight)
        , worldWidth(worldWidth)
        , worldHeight(worldHeight)
        , fSliceWidth(static_cast<float>(sliceWidth))
        , fSliceHeight(static_cast<float>(sliceHeight))
        , slicesPerScreen(-1)
        , sliceAtCenter(-1.0f)
        , screenCenter{ 0,0 }
    {
    }

    LevelTransformer()
        : sliceSize(0)
        , sliceWidth(0)
        , sliceHeight(0)
        , worldWidth(0)
        , worldHeight(0)
        , fSliceWidth(0)
        , fSliceHeight(0)
        , slicesPerScreen(-1)
        , sliceAtCenter(-1.0f)
        , screenCenter{ 0,0 }
    {
    }

    // convert image slice/index into slice to a point; gives left-top most world point of element
    Vector3 simToWorld(float slice, int index) const {
        // ensure index in range
        int modulo = sliceSize;
        while (index < 0) {
            index += modulo; // yikes
        }
        index = index % modulo;

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

    Vector3 simToWorldFloat(float slice, float index) const {
        float index1 = floorf(index);
        float t = index - index1;
        int index1Int = static_cast<int>(index1);

        Vector3 p0 = simToWorld(slice, index1Int);
        Vector3 p1 = simToWorld(slice, index1Int + 1);
        return Vector3Lerp(p0, p1, t);
    }

    Vector2 worldToScreen(const Vector3& world) const {
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

    Vector2 simToScreen(float slice, float index) const {
        return worldToScreen(simToWorldFloat(slice, index));
    }

    int sliceSize;
    int sliceWidth;
    int sliceHeight;
    float worldWidth;
    float worldHeight;
    float fSliceWidth;
    float fSliceHeight;
    
    float sliceAtCenter;
    float slicesPerScreen;
    Vector2 screenCenter;
};

class Explosion : public Thing
{
public:
    typedef struct SimElement {
        Vector2 dir;
        float t1;
        float t2;
        float vel1;
        float vel2;
        int count;
        Color color;

        SimElement() {
            vel1 = vel2 = t1 = t2 = 0.0f;
            dir = Vector2{ 0,0 };
            count = 0;
            static std::vector<Color> colors = { {124, 10, 2, 255}, {178, 34, 34, 255}, {226, 88, 34, 255}, {241, 188, 49}, {246, 240, 82} };
            
            color = colors[GetRandomValue(0, colors.size() - 1)];
        }
        void setDir(Vector2 dir) {
            this->dir = Vector2Normalize(dir);
        }

        void simit() {
            t1 += vel1;
            t2 += vel2;
            ++count;
        }

        void renderit(const Vector2& origin, LevelTransformer& transform) {
            Vector2 pa = Vector2Add(origin, Vector2Multiply(dir, { t1, t1 }));
            Vector2 pb = Vector2Add(origin, Vector2Multiply(dir, { t2, t2 }));

            Vector2 pas = transform.simToScreen(pa.x, pa.y);
            Vector2 pbs = transform.simToScreen(pb.x, pb.y);
            std::vector< Vector2 > pts;
            const int nExplosionPoints = 50;
            for (int ii = 0; ii < nExplosionPoints; ++ii) {
                float tt = float(ii) / 9;
                Vector2 ptSim = Vector2Lerp(pa, pb, tt);
                pts.push_back(transform.simToScreen(ptSim.x, ptSim.y));
            }
            unsigned char aa = 255;
            if (count*2 > 50) {
                aa = std::max(0, 255 + 50 - count*2);
            }
            color.a = aa;
            for (int ii = 0; ii < nExplosionPoints-1; ++ii) {
                DrawLineV(pts[ii], pts[ii+1], color);
            }
        }
    } SimElement;

    Explosion()
    {
        const int numShards = 100;
        for (int ii = 0; ii < numShards; ++ii) {
            add();
        }
    }

    void add() {
        float deg = RandFloat(360.0f);
        Vector2 dir = Vector2Rotate({ 1.0f, 0.0f }, deg);
        SimElement elem;
        elem.setDir(dir);
        elem.vel2 = RandFloat(1.3f);
        elem.vel1 = elem.vel2 * (0.8f + RandFloat(0.2f));
        explosionShards.push_back(elem);
    }
    void simit(int simTick) {
        for (auto& elem : explosionShards) {
            elem.simit();
        }
        if (explosionShards.size() < 1000)
            for (int pp = 0; pp < 30; ++pp) add();
    }

    void doRender() override {
        Vector2 originV{ origin.slice, origin.positionInSlice };
        for (auto& elem : explosionShards) {
            elem.renderit(originV, transform);
        }
    }

    LevelTransformer transform;
    SimSpacePosition origin;
    std::vector< SimElement > explosionShards;
};

class LevelGeometry : public Thing
{
public:
    LevelGeometry()
        : Thing()
        , playerPosition(static_cast<float>(sliceWidth + sliceWidth / 2 + sliceHeight))
        , playerColor({ 45,227,191,255 })
        , playerSlice(10.0f)
        , numSlices(3000)
        , dangerZone(startingDangerZone)
        , transformer(sliceSize, sliceWidth, sliceHeight, worldWidth, worldHeight)
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
                worldSlice[jj] = transformer.simToWorld(static_cast<float>(ii), jj);
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

    float dangerZoneT() {
        float t = 1.0f - (playerSlice - float(dangerZone)) / float(numWarningZones);
        return std::min(1.0f, std::max(0.0f, t));
    }
    void loadBackgroundImage(const char fname[]) {
        backgroundImage.reset(new SafeImage(fname));
    }

    // sim space constants
    int numSlices = 3000; // number of individual slices down the 'z' axis

    //          sliceWidth
    //    a------------------+
    //    b                  |
    //    |                  | sliceHeight
    //    +------------------+
    //
    //    a - index 0
    //    b - index sliceSize-1
    const int sliceWidth = 80; // number of pixels in x axis
    const int sliceHeight = 60; // number of pixels in y axis
    const int sliceSize = sliceWidth * 2 + sliceHeight * 2;

    const float playerHeightSliceDirDiv2 = 0.5f;
    const float playerWidthInSliceDiv2 = 0.5f;

    // grid space
    std::vector< std::vector<unsigned char> > geom;
    float playerSlice; // which slice the player is on
    float playerPosition; // position on slice

    // world space
    const float                         worldWidth = static_cast<float>(400);
    const float                         worldHeight = static_cast<float>(300);
    std::vector< std::vector<Vector3> > worldGeom; // position of top-left point of each geom pixel; includes extra row/element to bake end 

    // Display Constants
    Color playerColor;
    const float slicesBeforePlayer = 20.0f;
    const float slicesPerScreen = 30.0f;

    // Danger Zone
    int dangerZone; // zone in which player is killed
    const int startingDangerZone = -40;
    int numWarningZones = 50;

    // Visuals
    std::unique_ptr<SafeImage> backgroundImage;

    LevelTransformer transformer;

  private:

      void DrawPlayer(const Vector2& screenCenter, float sliceAtCenter)
      {
          SimSpacePosition sp0, sp1, sp2, sp3;
          playerCornersInSimSpace(playerSlice, playerPosition, playerWidthInSliceDiv2, playerHeightSliceDirDiv2, sp0, sp1, sp2, sp3);
          const Vector3 player0World = transformer.simToWorldFloat(sp0.slice, sp0.positionInSlice);
          const Vector3 player1World = transformer.simToWorldFloat(sp1.slice, sp1.positionInSlice);
          const Vector3 player2World = transformer.simToWorldFloat(sp2.slice, sp2.positionInSlice);
          const Vector3 player3World = transformer.simToWorldFloat(sp3.slice, sp3.positionInSlice);

          const Vector2 p0 = transformer.worldToScreen(player0World);
          const Vector2 p1 = transformer.worldToScreen(player1World);
          const Vector2 p2 = transformer.worldToScreen(player2World);
          const Vector2 p3 = transformer.worldToScreen(player3World);
          DrawTriangle(p0, p1, p2, playerColor);
          DrawTriangle(p0, p2, p3, playerColor);
      }

      void DrawAllGrid(const Vector2& screenCenter, float sliceAtCenter, std::function<void(int, int, Color&, bool&)> ColorCallback)
      {
          const int numSlicesToIterate = static_cast<int>(slicesPerScreen) + 2;
          const int sliceAtCenterInt = static_cast<int>(sliceAtCenter);

          for (int ii = 0; ii < numSlicesToIterate; ++ii) {
              int currSliceIndex = sliceAtCenterInt - ii;
              if (currSliceIndex >= 0 && currSliceIndex < (static_cast<int>(worldGeom.size()) - 1)) {
                  assert(size_t(currSliceIndex) < worldGeom.size());
                  const std::vector<Vector3>& sliceWorld = worldGeom[currSliceIndex];
                  const std::vector<Vector3>& nextSliceWorld = worldGeom[currSliceIndex + 1];
                  assert(sliceWorld.size() <= size_t(sliceSize + 1));
                  //       b11-p3--------p2-b12
                  //        |\   \xxxxxx/   /|
                  //        | a11-p0--p1-a12 |
                  //        |  |          |  |
                  //        | a21--------a22 |
                  //        |/              \|
                  //       b21--------------b22
                  for (int jj = 0; jj < sliceSize; jj++) {
                      const Vector2 p0 = transformer.worldToScreen(sliceWorld[jj]);
                      const Vector2 p1 = transformer.worldToScreen(sliceWorld[jj + 1]);
                      const Vector2 p2 = transformer.worldToScreen(nextSliceWorld[jj + 1]);
                      const Vector2 p3 = transformer.worldToScreen(nextSliceWorld[jj]);
                      Color col;
                      bool render;
                      ColorCallback(currSliceIndex, jj, col, render);
                      if (render) {
                          DrawTriangle(p2, p1, p0, col);
                          DrawTriangle(p3, p2, p0, col);
                      }
                  }
              }
          }
      }

      void DrawGridSolid(const Vector2& screenCenter, float sliceAtCenter, const Color& col)
      {
          const int numSlicesToIterate = static_cast<int>(slicesPerScreen) + 2;
          const int sliceAtCenterInt = static_cast<int>(sliceAtCenter);

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

                      const Vector2 p0 = transformer.worldToScreen(sliceWorld[jj]);
                      const Vector2 p1 = transformer.worldToScreen(sliceWorld[jj + 1]);
                      const Vector2 p2 = transformer.worldToScreen(nextSliceWorld[jj + 1]);
                      const Vector2 p3 = transformer.worldToScreen(nextSliceWorld[jj]);
                      DrawTriangle(p2, p1, p0, col);
                      DrawTriangle(p3, p2, p0, col);
                  }
              }
          }
      }
};

void LevelGeometry::doRender()
{
    const float centerX = static_cast<float>(GetScreenWidth() / 2);
    const float centerY = static_cast<float>(GetScreenHeight() / 2);
    const Vector2 screenCenter = { centerX, centerY };
    const float sliceAtCenter = playerSlice + slicesBeforePlayer;


    //DrawGrid(screenCenter, sliceAtCenter, [](int, int, Color& col, bool& render) {col = RED; render = true; });
    if (backgroundImage.get()) {
        DrawAllGrid(screenCenter, sliceAtCenter, [=](int slice, int sliceIndex, Color& col, bool& render) {
                render = false;
                if (slice >= 0 && slice < backgroundImage->image.width && sliceIndex >= 0 && sliceIndex < backgroundImage->image.height) {
                    col = backgroundImage->color(slice, sliceIndex);
                    render = true;
                }
            });
    }
    DrawGridSolid(screenCenter, sliceAtCenter, RED);
    DrawPlayer(screenCenter, sliceAtCenter);
    DrawAllGrid(screenCenter, sliceAtCenter, [=](int slice, int sliceIndex, Color& col, bool& render) {
            render = false;
            if (slice <= dangerZone) {
                col = RED;
                render = true;
                float wave = 0.2f * (0.5f * sinf(float(slice) / 3.0f + float(gSimTick) * 0.15f) + 0.5);
                col.a = static_cast<unsigned char>(255.0f * (0.3 + wave));
            }
            else if (slice - dangerZone <= numWarningZones) {
                float t = 1 - float(slice - dangerZone) / float(numWarningZones);
                col = YELLOW;
                render = true;
                float wave = 0.2f * (0.5f * sinf(float(slice)/3.0f + float(gSimTick) * 0.15f) + 0.5);
                col.a = static_cast<unsigned char>(255.0f * t * (0.3 + wave));
            }
        });
}

class GameState
{
public:
    enum class PlayerDirection { CCW, CW, IN, OUT, NONE };

    GameState()
        : playerDirection(PlayerDirection::IN)
        , simTick(0)
        , debugText(WHITE, {30, 30})
        , pausedText(WHITE, { 370, 300 }, "PAUSED")
        , paused(false)
        , playerDead(false)
        , transformer(lg.transformer)
        , noKill(false)
    {
        lg.loadLevelFromImage("Content/test_level.png");
        lg.loadBackgroundImage("Content/test_level_bg.png");
        bump = LoadSound("Content/hitwall.wav");
        dangerSound = LoadSound("Content/bg_buzz.wav");
        deathSound = LoadSound("Content/explosion.wav");

        debugText.display = false;
        pausedText.display = false;
    }

    ~GameState()
    {
        UnloadSound(bump);
    }

    void Sim(float simTimeSeconds) {
        gSimTick = simTick;
        const float playerHorizontalSpeed = 0.75;
        const float playerVerticalSpeed = 0.5;

        if (IsKeyPressed(KEY_K)) noKill = !noKill;
        if (IsKeyPressed(KEY_D)) debugText.display = !debugText.display;
        if (IsKeyPressed(KEY_P)) {
            paused = !paused;
            pausedText.display = paused;
        }
        if (paused)
            return;

        if (playerDead) {
            explosion.simit(simTick);
            return;
        }
        
        if (IsKeyDown(KEY_UP)) playerDirection = PlayerDirection::IN;
        if (IsKeyDown(KEY_DOWN)) playerDirection = PlayerDirection::OUT;
        if (IsKeyDown(KEY_LEFT)) playerDirection = PlayerDirection::CCW;
        if (IsKeyDown(KEY_RIGHT)) playerDirection = PlayerDirection::CW;
        bool wasCollision = false;
        if (playerDirection == PlayerDirection::IN) {
            wasCollision = lg.incrPlayerSlice(playerVerticalSpeed);
        }
        else if (playerDirection == PlayerDirection::OUT) {
            wasCollision = lg.incrPlayerSlice(-playerVerticalSpeed);
        }
        else if (playerDirection == PlayerDirection::CCW) {
            wasCollision = lg.incrPlayerPosition(-playerHorizontalSpeed);
        }
        else if (playerDirection == PlayerDirection::CW) {
            wasCollision = lg.incrPlayerPosition(playerHorizontalSpeed);
        }
        if (wasCollision) {
            PlaySound(bump);
            playerDirection = PlayerDirection::NONE;
        }

        if (simTick % 12 == 0) {
            lg.dangerZone++;
        }

        float dangerValue = lg.dangerZoneT();
        //std::cout << GetMusicTimePlayed(dangerSound) << std::endl;
        if (dangerValue < 0.01f) {
            if (IsSoundPlaying(dangerSound))
                StopSound(dangerSound);
            SetSoundVolume(dangerSound, 0.0f);
        }
        else {
            if (! IsSoundPlaying(dangerSound))
                PlaySound(dangerSound);
            SetSoundVolume(dangerSound, dangerValue * 0.4f);
        }

        if (!noKill && static_cast<int>(floorf(lg.playerSlice)) <= lg.dangerZone) {
            playerDead = true;
            explosion.origin = SimSpacePosition(lg.playerSlice, lg.playerPosition);
            PlaySound(deathSound);
        }


        std::stringstream ss;
        ss << "Sim tick " << simTick << ", Player: (" << lg.playerSlice << ", " << lg.playerPosition << ")";
        debugText.text = ss.str();
        simTick++;
    }

    void Render() {
        const float centerX = static_cast<float>(GetScreenWidth() / 2);
        const float centerY = static_cast<float>(GetScreenHeight() / 2);
        transformer.screenCenter = Vector2{ centerX, centerY };
        transformer.slicesPerScreen = lg.slicesPerScreen;
        transformer.sliceAtCenter = lg.playerSlice + lg.slicesBeforePlayer;

        lg.transformer = transformer;
        explosion.transform = transformer;

        BeginDrawing();
        ClearBackground(BLACK);

        lg.render();
        debugText.render();
        pausedText.render();
        if (playerDead) explosion.render();

        EndDrawing();
    }

    int simTick;
    LevelGeometry lg;
    PlayerDirection playerDirection;
    Text debugText;
    Text pausedText;
    bool paused;
    bool playerDead;
    bool noKill;

    Sound bump;
    Sound dangerSound;
    Sound deathSound;
    LevelTransformer transformer;
    Explosion explosion;
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
    SetMasterVolume(0.5);
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        gameState.Sim(simTimeSeconds);
        gameState.Render();
    }

    CloseWindow();        // Close window and OpenGL context
    CloseAudioDevice();
    return 0;
}


