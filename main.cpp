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

#include "raylib.h"
#include "raymath.h"
#include <sstream>
#include <vector>
#include <memory>


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

class LevelGeometry : public Thing
{
public:
    LevelGeometry()
        : Thing()
        , slicePosition(0)
        , playerPosition(sliceWidth + sliceWidth / 2 + sliceHeight)
        , playerColor({ 45,227,191,255 })
    {
        geom.resize(numSlices);
        for (int ii = 0; ii < numSlices; ++ii) {
            geom[ii].resize(sliceSize);
            unsigned char sliceVal = 0;
            if (ii % 20 == 0) {
                sliceVal = 255;
            }
            for (int jj = 0; jj < sliceSize; jj++) {
                geom[ii][jj] = (ii % 10 == 0) ? (((jj % 10)<7) ? 0 : 255) : 0;
            }
        }
    }

    void incrPlayerPosition(int val) {
        const int modulo = 2 * sliceWidth + 2 * sliceHeight;
        playerPosition = (playerPosition + modulo + val) % modulo;
    }

    inline void drawTopSquare(const Vector2& a11, const Vector2& a22, const Vector2& aDelta,
        const Vector2& b11, const Vector2& b22, const Vector2& bDelta,
        const float t1, const float t2,
        const Color& color) {

        Vector2 p11 = { b11.x + t1 * bDelta.x, b11.y };
        Vector2 p12 = { b11.x + t2 * bDelta.x, b11.y };
        Vector2 p21 = { a11.x + t1 * aDelta.x, a11.y };
        Vector2 p22 = { a11.x + t2 * aDelta.x, a11.y };
        DrawTriangle(p11, p21, p22, color);
        DrawTriangle(p11, p22, p12, color);
    }

    inline void drawRightSquare(const Vector2& a11, const Vector2& a22, const Vector2& aDelta,
        const Vector2& b11, const Vector2& b22, const Vector2& bDelta,
        const float t1, const float t2, const Color& color) {

        Vector2 p11 = { a22.x, a11.y + t1 * aDelta.y };
        Vector2 p12 = { b22.x, b11.y + t1 * bDelta.y };
        Vector2 p21 = { a22.x, a11.y + t2 * aDelta.y };
        Vector2 p22 = { b22.x, b11.y + t2 * bDelta.y };
        DrawTriangle(p11, p21, p22, color);
        DrawTriangle(p11, p22, p12, color);
    }

    inline void drawBottomSquare(const Vector2& a11, const Vector2& a22, const Vector2& aDelta,
        const Vector2& b11, const Vector2& b22, const Vector2& bDelta,
        const float t1, const float t2, const Color& color) {

        Vector2 p11 = { a11.x + (1.0f - t2) * aDelta.x, a22.y };
        Vector2 p12 = { a11.x + (1.0f - t1) * aDelta.x, a22.y };
        Vector2 p21 = { b11.x + (1.0f - t2) * bDelta.x, b22.y };
        Vector2 p22 = { b11.x + (1.0f - t1) * bDelta.x, b22.y };
        DrawTriangle(p11, p21, p22, color);
        DrawTriangle(p11, p22, p12, color);
    }

    inline void drawLeftSquare(const Vector2& a11, const Vector2& a22, const Vector2& aDelta,
        const Vector2& b11, const Vector2& b22, const Vector2& bDelta,
        const float t1, const float t2, const Color& color) {

        Vector2 p11 = { b11.x, b11.y + (1.0f - t2) * bDelta.y };
        Vector2 p12 = { a11.x, a11.y + (1.0f - t2) * aDelta.y };
        Vector2 p21 = { b11.x, b11.y + (1.0f - t1) * bDelta.y };
        Vector2 p22 = { a11.x, a11.y + (1.0f - t1) * aDelta.y };
        DrawTriangle(p11, p21, p22, color);
        DrawTriangle(p11, p22, p12, color);
    }

    virtual void doRender() override
    {
        float centerX = static_cast<float>(GetScreenWidth() / 2);
        float centerY = static_cast<float>(GetScreenHeight() / 2);
        float dx = centerX / static_cast<float>(numLevels);
        float dy = centerY / static_cast<float>(numLevels);
        int actualSlice = std::max(0, std::min(slicePosition, numSlices - numLevels - 1));
        for (int ii = 0; ii < numLevels; ++ii) {
            int currSliceIndex = actualSlice - ii;
            if (currSliceIndex < 0)
                continue;

            const std::vector<unsigned char>& slice = geom[currSliceIndex];
            const float iifl = float(ii);
            const float iifl2 = float(ii + 1);
            const Vector2 a11 = { centerX - iifl * dx, centerY - iifl * dy };
            const Vector2 a22 = { centerX + iifl * dx, centerY + iifl * dy };
            const Vector2 aDelta = Vector2Subtract(a22, a11);
            const Vector2 b11 = { centerX - iifl2 * dx, centerY - iifl2 * dy };
            const Vector2 b22 = { centerX + iifl2 * dx, centerY + iifl2 * dy };
            const Vector2 bDelta = Vector2Subtract(b22, b11);
            //       b11--------------b12
            //        |\              /|
            //        | a11--------a12 |
            //        |  |          |  |
            //        | a21--------a22 |
            //        |/              \|
            //       b21--------------b22

            for (int jj = 0; jj < sliceWidth; jj++) {
                if (slice[jj]) {
                    float t1 = float(jj) / float(sliceWidth - 1);
                    float t2 = float(jj + 1) / float(sliceWidth - 1);
                    drawTopSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, RED);
                }
            }

            for (int jj = 0; jj < sliceHeight; jj++) {
                int base = sliceWidth;
                if (slice[base + jj]) {
                    float t1 = float(jj) / float(sliceHeight - 1);
                    float t2 = float(jj + 1) / float(sliceHeight - 1);
                    drawRightSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, RED);
                }
            }

            for (int jj = 0; jj < sliceWidth; jj++) {
                int base = sliceWidth + sliceHeight;
                if (slice[jj]) {
                    float t1 = float(jj) / float(sliceWidth - 1);
                    float t2 = float(jj + 1) / float(sliceWidth - 1);
                    drawBottomSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, RED);
                }
            }

            for (int jj = 0; jj < sliceHeight; jj++) {
                int base = sliceWidth*2 + sliceHeight;
                if (slice[base + jj]) {
                    float t1 = float(jj) / float(sliceHeight - 1);
                    float t2 = float(jj + 1) / float(sliceHeight - 1);
                    drawLeftSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, RED);
                }
            }

            if (ii == playerSlice) {
                int testPlayerPosition = playerPosition;

                if (testPlayerPosition < sliceWidth) {
                    float t1 = float(testPlayerPosition) / float(sliceWidth - 1);
                    float t2 = float(testPlayerPosition + 1) / float(sliceWidth - 1);
                    drawTopSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, playerColor);
                }
                else {
                    testPlayerPosition -= sliceWidth;
                    if (testPlayerPosition < sliceHeight) {
                        float t1 = float(testPlayerPosition) / float(sliceHeight - 1);
                        float t2 = float(testPlayerPosition + 1) / float(sliceHeight - 1);
                        drawRightSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, playerColor);
                    }
                    else {
                        testPlayerPosition -= sliceHeight;
                        if (testPlayerPosition < sliceWidth) {
                            float t1 = float(testPlayerPosition) / float(sliceWidth - 1);
                            float t2 = float(testPlayerPosition + 1) / float(sliceWidth - 1);
                            drawBottomSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, playerColor);
                        }
                        else {
                            testPlayerPosition -= sliceWidth;
                            if (testPlayerPosition < sliceHeight) {
                                float t1 = float(testPlayerPosition) / float(sliceHeight - 1);
                                float t2 = float(testPlayerPosition + 1) / float(sliceHeight - 1);
                                drawLeftSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, playerColor);
                            }
                        }
                    }
                }
            }
        }
    }
/*
    // convert image slice/index into slice to a point; gives left-top most world point of element
    Vector3 toWorld(int slice, int index) {
        // ensure index in range
        int modulo = sliceWidth * 2 + sliceHeight + 2;
        while (index < 0) {
            index += modulo; // yikes
        }
        index = index % modulo;

        const float fSliceWidth = static_cast<float>(sliceWidth);
        const float fSliceHeight = static_cast<float>(sliceHeight);
        if (index < sliceWidth) {
            float t1 = float(index) / fSliceWidth;
            return(Vector3{ worldWidth * t1 / fSliceWidth, 0.0f, static_cast<float>(slice) });
        }

        testPlayerPosition -= sliceWidth;
        if (testPlayerPosition < sliceHeight) {
            float t1 = float(index) / fSliceHeight;
            return(Vector3{ worldWidth, worldHeight * t1 / fSliceHeight, 0.0f, static_cast<float>(slice) });
        }

        testPlayerPosition -= sliceHeight;
        if (testPlayerPosition < sliceHeight) {
            float t1 = 1.0f - float(index) / fSliceWidth;
            return(Vector3{ worldWidth * t1 / fSliceWidth, fSliceHeight, static_cast<float>(slice) });
            }

                    float t1 = float(testPlayerPosition) / float(sliceWidth - 1);
                    float t2 = float(testPlayerPosition + 1) / float(sliceWidth - 1);
                    drawBottomSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, playerColor);
                }
                else {
                    testPlayerPosition -= sliceWidth;
                    if (testPlayerPosition < sliceHeight) {
                        float t1 = float(testPlayerPosition) / float(sliceHeight - 1);
                        float t2 = float(testPlayerPosition + 1) / float(sliceHeight - 1);
                        drawLeftSquare(a11, a22, aDelta, b11, b22, bDelta, t1, t2, playerColor);
                    }
                }
            }
        }
    }
    */
    // grid space
    std::vector< std::vector<unsigned char> > geom;
    // float playerSlice;
    //float playerPosition; // position on slice
    int playerPosition; // position on slice

    // items in world space
    const int                           worldWidth = static_cast<float>(sliceWidth);
    const int                           worldHeight = static_cast<float>(sliceHeight);
    std::vector< std::vector<Vector3> > worldGeom; // position of top-left point of each geom pixel; includes extra row/element to bake end 


    int slicePosition;
    const int numSlices = 3000;
    const int sliceWidth = 80;
    const int sliceHeight = 60;
    const int sliceSize = sliceWidth * 2 + sliceHeight * 2;
    const int numLevels = 46;
    const int playerSlice = numLevels - 10;
    Color playerColor;
};



class GameState
{
public:
    typedef enum PlayerDirection { CCW, CW, IN, OUT } PlayerDirection;

    GameState()
      : playerDirection(IN)
      , simTick(0)
    {
    }

    void Sim(float simTimeSeconds) {
        if (IsKeyDown(KEY_UP)) playerDirection = IN;
        if (IsKeyDown(KEY_DOWN)) playerDirection = OUT;
        if (IsKeyDown(KEY_LEFT)) playerDirection = CCW;
        if (IsKeyDown(KEY_RIGHT)) playerDirection = CW;
        if (playerDirection == IN) {
            if (simTick%3==0) lg.slicePosition++;
        }
        else if (playerDirection == OUT) {
            if (simTick % 3 == 0) lg.slicePosition--;
        }
        else if (playerDirection == CCW) {
            lg.incrPlayerPosition(-1);
        }
        else if (playerDirection == CW) {
            lg.incrPlayerPosition(1);
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
    SetTargetFPS(fps);

    // Main game loop
    GameState gameState;
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        gameState.Sim(simTimeSeconds);
        gameState.Render();
    }

    CloseWindow();        // Close window and OpenGL context

    return 0;
}


