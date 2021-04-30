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
* emcc main2.cpp -s WASM=1 -o pixin.html -L lib -I include -l raylib -s USE_GLFW=3 -s ASYNCIFY
* python -m http.server 7801
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
    Text(const Color& color, const Vector2& position = { 0 }, const std::string str = "", int fontSize = 20, bool center = true)
        : Thing(position)
        , text(str)
        , fontSize(fontSize)
        , center(center)
        , color(color) {}

    virtual void doRender() override
    {
        if (center) {
            int width = MeasureText(text.c_str(), fontSize);
            DrawText(text.c_str(), int(position.x)-width/2, int(position.y), fontSize, color);
        }
        else {
            DrawText(text.c_str(), int(position.x), int(position.y), fontSize, color);
        }
    }

    std::string text;
    int fontSize;
    Color color;
    bool center;
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

typedef struct GridPosition {
    GridPosition(int slice = 0, int positionInSlice = 0)
        : slice(slice)
        , positionInSlice(positionInSlice) {}
    bool operator==(const GridPosition& gp) const { return slice == gp.slice && positionInSlice == gp.positionInSlice; }
    int slice;
    int positionInSlice;
} GridPosition;

void playerCornersInSimSpace(
    float playerSlice, float playerPosition, float playerWidthInSliceDiv2, float playerHeightSliceDirDiv2,
    SimSpacePosition& p0, SimSpacePosition& p1, SimSpacePosition& p2, SimSpacePosition& p3)
{
    p0 = { playerSlice + playerHeightSliceDirDiv2, playerPosition - playerWidthInSliceDiv2 };
    p1 = { playerSlice + playerHeightSliceDirDiv2, playerPosition + playerWidthInSliceDiv2 };
    p2 = { playerSlice - playerHeightSliceDirDiv2, playerPosition + playerWidthInSliceDiv2 };
    p3 = { playerSlice - playerHeightSliceDirDiv2, playerPosition - playerWidthInSliceDiv2 };
}



bool inGridRange(const GridPosition& pos, const GridPosition& gp1, const GridPosition& gp2)
{
    if (gp1.slice == INT_MIN && gp2.slice == INT_MIN)
        return false;

    GridPosition minGp(std::min(gp1.slice, gp2.slice), std::min(gp1.positionInSlice, gp2.positionInSlice));
    GridPosition maxGp(std::max(gp1.slice, gp2.slice), std::max(gp1.positionInSlice, gp2.positionInSlice));
    return pos.slice >= minGp.slice && pos.slice <= maxGp.slice && pos.positionInSlice >= minGp.positionInSlice && pos.positionInSlice <= maxGp.positionInSlice;
}

bool havePath(const GridPosition& aa, const GridPosition& bb,
    const std::vector< std::vector< unsigned char > >& geom,
    int sliceSize, int startSlice, int endSlice, GridPosition tempWallA = { INT_MIN, INT_MIN }, GridPosition tempWallB = { INT_MIN, INT_MIN }) {
    assert(! geom[aa.slice][aa.positionInSlice]);
    assert(! geom[bb.slice][bb.positionInSlice]);
    assert(aa.slice >= startSlice && aa.slice <= endSlice);
    assert(bb.slice >= startSlice && bb.slice <= endSlice);

    std::vector< bool > visited((endSlice - startSlice + 1) * sliceSize, false);
    std::vector< GridPosition > candidates;

    auto visitedArrayElem = [startSlice, endSlice, sliceSize](const GridPosition& position) -> int {
        assert(position.slice >= startSlice && position.slice <= endSlice);
        return (position.slice - startSlice) * sliceSize + position.positionInSlice;
    };

    auto testAndAdd = [&](const GridPosition& newPos) {
        if (newPos.slice > endSlice || newPos.slice < startSlice ||
            visited[visitedArrayElem(newPos)] || geom[newPos.slice][newPos.positionInSlice] ||
            inGridRange(newPos, tempWallA, tempWallB))
            return;
        candidates.push_back(newPos);
    };

    visited[visitedArrayElem(aa)] = true;
    candidates.push_back(aa);
    bool pathExists = false;
    while (!pathExists && !candidates.empty()) {
        GridPosition pos = candidates.back();
        candidates.pop_back();
        visited[visitedArrayElem(pos)] = true;
        if (pos == bb) {
            pathExists = true;
            continue;
        }
        testAndAdd(GridPosition(pos.slice + 1, pos.positionInSlice));
        testAndAdd(GridPosition(pos.slice - 1, pos.positionInSlice));
        testAndAdd(GridPosition(pos.slice, (pos.positionInSlice - 1 + sliceSize) % sliceSize));
        testAndAdd(GridPosition(pos.slice, (pos.positionInSlice + 1) % sliceSize));
    }
    return pathExists;
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
        , playerPosition(static_cast<float>(sliceWidth / 2))
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

    virtual void doRender() override;

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
        winningZone = int(geom.size()) - 100;
        updateWorldGeom();
        UnloadImageColors(colors);
        UnloadImage(levelImage);
    }


    void setGridRange(std::vector< std::vector< unsigned char > >& geom, int startSlice, int endSlice, const GridPosition& gp1, const GridPosition& gp2, unsigned char value = 0, int quantize = 1)
    {
        GridPosition minGp(std::min(gp1.slice, gp2.slice), std::min(gp1.positionInSlice, gp2.positionInSlice));
        GridPosition maxGp(std::max(gp1.slice, gp2.slice), std::max(gp1.positionInSlice, gp2.positionInSlice));
        //std::cout << minGp.slice << ", " << minGp.positionInSlice << " -> " << maxGp.slice << ", " << maxGp.positionInSlice << std::endl;
        for (int ii = minGp.slice; ii <= maxGp.slice; ++ii) {
            if (ii < startSlice || ii > endSlice)
                continue;
            for (int jj = minGp.positionInSlice; jj <= maxGp.positionInSlice; ++jj) {
                int slice = ii;
                int pos = (jj + sliceSize) % sliceSize;
                if (quantize > 1) {
                    pos = pos / quantize;
                    pos *= quantize;
                }
                geom[slice][pos] = value;
            }
        }
    }

    void generateMaze(std::vector< std::vector< unsigned char > >& geom, int startSlice, int endSlice, int sliceRange, int posRange, int width) {
        assert(havePath(GridPosition(startSlice - 1, 0), GridPosition(endSlice + 1, 0), geom, sliceSize, startSlice - 1, endSlice + 1));
        for (int ii = startSlice; ii <= endSlice; ++ii) {
            for (int jj = 0; jj < sliceSize; ++jj) {
                geom[ii][jj] = 255;
            }
        }
        assert(!havePath(GridPosition(startSlice - 1, 0), GridPosition(endSlice + 1, 0), geom, sliceSize, startSlice - 1, endSlice + 1));

        const int quantize = 2 * width + 3;
        while (!havePath(GridPosition(startSlice - 1, 0), GridPosition(endSlice + 1, 0), geom, sliceSize, startSlice - 1, endSlice + 1)) {
            GridPosition gp(GetRandomValue(startSlice, endSlice), GetRandomValue(0, sliceSize - 1));
            if (GetRandomValue(0, 10)) {
                int randVal = GetRandomValue(2, posRange);
                int secondPos = gp.positionInSlice + randVal;
                setGridRange(
                    geom, startSlice, endSlice, GridPosition(gp.slice - width, gp.positionInSlice),
                    GridPosition(gp.slice + width, secondPos), 0, 1);
            }
            else {
                int secondSlice = std::min(gp.positionInSlice + GetRandomValue(1, sliceRange), endSlice);
                setGridRange(
                    geom, startSlice, endSlice, GridPosition(gp.slice, gp.positionInSlice - width),
                    GridPosition(secondSlice, gp.positionInSlice + width), 0, 1);
            }
        }
    }

    void generateMaze2(std::vector< std::vector< unsigned char > >& geom, int startSlice, int endSlice, int maxNumLines = 50, int quantizeSlice = 3, int quantizePos = 10) {
        assert(havePath(GridPosition(startSlice - 1, 0), GridPosition(endSlice + 1, 0), geom, sliceSize, startSlice - 1, endSlice + 1));
        for (int ii = startSlice; ii <= endSlice; ++ii) {
            for (int jj = 0; jj < sliceSize; ++jj) {
                geom[ii][jj] = 0;
            }
        }

        int nLines = 0;
        const int pQ = quantizePos;
        const int sQ = quantizeSlice;
        const int pW = 1;
        const int sW = 0;
        GridPosition startPos(startSlice - 1, 0);
        GridPosition endPos(endSlice + 1, 0);

        while (nLines < maxNumLines) {
            ++nLines;
            GridPosition newWallP0;
            GridPosition newWallP1;
            if (GetRandomValue(0, sliceSize + 150) < sliceSize) {
                int pos1 = (GetRandomValue(0, sliceSize) / pQ) * pQ;
                int pos2 = pos1 + (GetRandomValue(quantizePos * 3, sliceSize/8) / pQ) * pQ;
                int slice = (GetRandomValue(startSlice, endSlice) / sQ) * sQ;
                newWallP0 = GridPosition(slice - sW, pos1);
                newWallP1 = GridPosition(slice + sW, pos2);
            }
            else {
                int slice1 = (GetRandomValue(startSlice, endSlice) / sQ) * sQ;
                int slice2 = slice1 + (GetRandomValue(1, 20) / sQ) * sQ;
                int pos = (GetRandomValue(0, sliceSize) / pQ) * pQ;
                newWallP0 = GridPosition(slice1, pos - pW);
                newWallP1 = GridPosition(slice2, pos + pW);
            }
            if (havePath(startPos, endPos, geom, sliceSize, startSlice - 1, endSlice + 1, newWallP0, newWallP1)) {
                setGridRange(geom, startSlice, endSlice, newWallP0, newWallP1, 255);
                assert(havePath(startPos, endPos, geom, sliceSize, startSlice - 1, endSlice + 1));
            }
                
        }
    }

    void generateSlip(std::vector< std::vector< unsigned char > >& geom, int startSlice, int endSlice, const std::vector<int>& positions, int slipWidth, bool fill = true) {
        if (fill) {
            for (int ii = startSlice; ii <= endSlice; ++ii) {
                for (int jj = 0; jj < sliceSize; ++jj) {
                    geom[ii][jj] = 255;
                }
            }
        }

        const int slipWidthDiv2 = slipWidth / 2;
        for (const auto& sp : positions) {
            for (int ii = startSlice; ii <= endSlice; ++ii) {
                const int start = sp - slipWidthDiv2;
                const int end = start + slipWidth;
                for (int jj = start; jj <= end; ++jj) {
                    const int posInSlice = (jj + sliceSize) % sliceSize;
                    geom[ii][posInSlice] = 0;
                }
            }
        }
    }

    void generateRandoWithSlip(std::vector< std::vector< unsigned char > >& geom, int startSlice, int endSlice, int nPoints, int nSlips, int slipWidth) {
        for (int ii = startSlice; ii < endSlice; ++ii) {
            for (int jj = 0; jj < sliceSize; ++jj) {
                geom[ii][jj] = 0;
            }
        }

        for (int ii = 0; ii < nPoints; ++ii) {
            geom[GetRandomValue(startSlice, endSlice)][GetRandomValue(0, sliceSize - 1)] = 255;
        }

        std::vector<int> randomSlipSpots;
        for (int ii = 0; ii < nSlips; ++ii) {
            randomSlipSpots.push_back(GetRandomValue(0, sliceSize - 1));
        }
        generateSlip(geom, startSlice, endSlice, randomSlipSpots, slipWidth, false);
    }

    void generateLevel() {
        numSlices = 2000;
        geom.resize(numSlices);
        for (int ii = 0; ii < numSlices; ++ii) {
            geom[ii].resize(sliceSize);
            for (int jj = 0; jj < sliceSize; ++jj) {
                geom[ii][jj] = 0;
            }
        }

        auto everyN = [](int n, int sliceSize) -> std::vector<int> {
            std::vector<int> result;
            for (int ii = 0; ii < sliceSize; ii += n) {
                result.push_back(ii);
            }
            return result;
        };
        std::vector< int > centerPositions{ sliceWidth / 2, sliceWidth + sliceHeight / 2, sliceWidth / 2 + sliceWidth + sliceHeight, sliceWidth * 2 + sliceHeight + sliceHeight / 2 };
        std::vector< int > cornerPositions{ 0, sliceWidth, sliceWidth + sliceHeight, 2 * sliceWidth + sliceHeight };
        int currSlice = 50;

        for (int ii = 0; ii < 5; ++ii) {
            generateSlip(geom, currSlice, currSlice, centerPositions, 40);
            currSlice += 4;
        }
        for (int ii = 0; ii < 5; ++ii) {
            generateSlip(geom, currSlice, currSlice, everyN(30, sliceSize), 10);
            currSlice += 4;
        }
        currSlice += 10;
        for (int ii = 0; ii < 5; ++ii) {
            generateSlip(geom, currSlice, currSlice, centerPositions, 45);
            currSlice += 4;
            generateSlip(geom, currSlice, currSlice, cornerPositions, 60);
            currSlice += 4;
        }

        currSlice += 10;
        for (int ii = 0; ii < 20; ++ii) {
            generateSlip(geom, currSlice, currSlice, everyN(50 + ii / 2, sliceSize + ii), 10);
            currSlice += 4;
        }

        for (int ii = 0; ii < 5; ++ii) {
            generateRandoWithSlip(geom, currSlice, currSlice + 10, 50, 3, 15);
            currSlice += 30;
        }
        currSlice += 20;
        generateRandoWithSlip(geom, currSlice, currSlice + 100, 100, 3, 15);
        currSlice += 130;

        //        void generateMaze2(std::vector< std::vector< unsigned char > >&geom, int startSlice, int endSlice, int maxNumLines = 50, int quantizeSlice = 3, int quantizePos = 10) {
        //        generateMaze2(geom, currSlice, currSlice + 50, 200);
        //        currSlice += 70;

        //        generateMaze2(geom, currSlice, currSlice + 150, 2000);
        //        currSlice += 170;

        generateMaze2(geom, currSlice, currSlice + 150, 200, 10, 20);
        currSlice += 170;

        generateMaze2(geom, currSlice, currSlice + 50, 120, 5, 20);
        currSlice += 60;
        generateMaze2(geom, currSlice, currSlice + 50, 120, 5, 20);
        currSlice += 60;
        generateMaze2(geom, currSlice, currSlice + 50, 120, 5, 20);
        currSlice += 60;
        generateMaze2(geom, currSlice, currSlice + 50, 120, 5, 20);
        currSlice += 60;

        currSlice += 40;
        winningZone = currSlice;
        updateWorldGeom();
    }

    void generateLevel2() {
        numSlices = 2000;
        geom.resize(numSlices);
        for (int ii = 0; ii < numSlices; ++ii) {
            geom[ii].resize(sliceSize);
            for (int jj = 0; jj < sliceSize; ++jj) {
                geom[ii][jj] = 0;
            }
        }

        int currSlice = 50;

        for (int ii = 0; ii < 10; ++ii) {
            generateMaze2(geom, currSlice, currSlice + 50, 60, 5, 20);
            currSlice += 60;
        }

        currSlice += 40;
        winningZone = currSlice;
        updateWorldGeom();
    }

    void generateLevel3() {
        numSlices = 2000;
        geom.resize(numSlices);
        for (int ii = 0; ii < numSlices; ++ii) {
            geom[ii].resize(sliceSize);
            for (int jj = 0; jj < sliceSize; ++jj) {
                geom[ii][jj] = 0;
            }
        }

        int currSlice = 50;

        generateMaze2(geom, currSlice, currSlice + 500, 800, 8, 20);
        currSlice += 520;

        currSlice += 40;
        winningZone = currSlice;
        updateWorldGeom();
    }

    bool collides(float testPlayerSlice, float testPlayerPosition) {
        auto testSinglePoint = [=](const SimSpacePosition& spp) -> bool {
            //return false; //TODO: COMMENT THIS LINE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
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

    void drawBackground() {
        if (backgroundImage.get()) {
            DrawAllGrid(transformer.screenCenter, transformer.sliceAtCenter, [=](int slice, int sliceIndex, Color& col, bool& render) {
                render = false;
                if (slice >= 0 && slice < backgroundImage->image.width && sliceIndex >= 0 && sliceIndex < backgroundImage->image.height) {
                    col = backgroundImage->color(slice, sliceIndex);
                    render = true;
                }
                });
        }
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
    int winningZone;

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
    drawBackground();
    DrawGridSolid(screenCenter, sliceAtCenter, RED);
    DrawPlayer(screenCenter, sliceAtCenter);
    DrawAllGrid(screenCenter, sliceAtCenter, [=](int slice, int sliceIndex, Color& col, bool& render) {
            render = false;
            if (slice <= dangerZone) {
                col = RED;
                render = true;
                float wave = 0.1f * (0.5f * sinf(float(slice) / 3.0f + float(gSimTick) * 0.4f) + 0.5f);
                col.a = static_cast<unsigned char>(255.0f * (0.3 + wave));
            }
            else if (slice >= winningZone) {
                col = GREEN;
                render = true;
                float wave = 0.1f * (0.5f * sinf(float(slice) / 3.0f + float(gSimTick) * 0.4f) + 0.5f);
                col.a = static_cast<unsigned char>(255.0f * (0.3 + wave));
            }
            else if (slice - dangerZone <= numWarningZones) {
                float t = 1 - float(slice - dangerZone) / float(numWarningZones);
                col = YELLOW;
                render = true;
                float wave = 0.1f * (0.5f * sinf(float(slice)/3.0f + float(gSimTick) * 0.4f) + 0.5f);
                col.a = static_cast<unsigned char>(255.0f * t * (0.3 + wave));
            }
        });
}

class GameState
{
public:
    GameState()
        : finished(false)
    {
        if (!gsInit) {
            bump = LoadSound("Content/hitwall.wav");
            dangerSound = LoadSound("Content/bg_buzz.wav");
            deathSound = LoadSound("Content/explosion.wav");
            winSound = LoadSound("Content/win.wav");
        }
    }
    virtual void Sim(float simTimeSeconds) = 0;
    virtual void Render() = 0;

    // not thread safe
    bool finished;
    static Sound bump;
    static Sound winSound;
    static Sound dangerSound;
    static Sound deathSound;

private:
    static bool gsInit;
};

bool GameState::gsInit = false;
Sound GameState::bump;
Sound GameState::winSound;
Sound GameState::dangerSound;
Sound GameState::deathSound;

class LevelGameState : public GameState
{
public:
    enum class PlayerDirection { CCW, CW, IN, OUT, NONE };

    LevelGameState(int level = 0)
        : playerDirection(PlayerDirection::IN)
        , simTick(0)
        , debugText(WHITE, {30, 30})
        , pausedText(WHITE, { 370, 300 }, "PAUSED")
        , paused(false)
        , playerDead(false)
        , transformer(lg.transformer)
        , noKill(false)
        , gameWon(false)
        , finalCountdown(-1)
    {
        //lg.loadLevelFromImage("Content/test_level.png");
        if (level == 0) {
            lg.generateLevel();
        }
        else if (level == 1) {
            lg.generateLevel2();
        }
        else {
            lg.generateLevel3();
        }
        lg.loadBackgroundImage("Content/test_level_bg.png");

        debugText.display = false;
        pausedText.display = false;
    }

    ~LevelGameState()
    {
    }

    void Sim(float simTimeSeconds) override {
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

        if (finalCountdown == 0) {
            finished = true;
        }
        else if (finalCountdown > 0) {
            --finalCountdown;
        }
        if (finalCountdown < 0 && (gameWon || playerDead)) {
            finalCountdown = 60 * 5; // 5 seconds
        }

        if (playerDead) {
            explosion.simit(simTick);
            return;
        }
 
        if (classicControls) {
            if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) playerDirection = PlayerDirection::IN;
            if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) playerDirection = PlayerDirection::OUT;
            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) playerDirection = PlayerDirection::CCW;
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) playerDirection = PlayerDirection::CW;
        }
        else {
            if (lg.playerPosition < lg.sliceWidth) {
                if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) playerDirection = PlayerDirection::OUT;
                if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) playerDirection = PlayerDirection::IN;
                if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) playerDirection = PlayerDirection::CCW;
                if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) playerDirection = PlayerDirection::CW;
            }
            else if (lg.playerPosition < lg.sliceWidth + lg.sliceHeight) {
                if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) playerDirection = PlayerDirection::CCW;
                if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) playerDirection = PlayerDirection::CW;
                if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) playerDirection = PlayerDirection::IN;
                if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) playerDirection = PlayerDirection::OUT;
            }
            else if (lg.playerPosition < 2*lg.sliceWidth + lg.sliceHeight) {
                if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) playerDirection = PlayerDirection::IN;
                if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) playerDirection = PlayerDirection::OUT;
                if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) playerDirection = PlayerDirection::CW;
                if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) playerDirection = PlayerDirection::CCW;
            }
            else {
                if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) playerDirection = PlayerDirection::CW;
                if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) playerDirection = PlayerDirection::CCW;
                if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) playerDirection = PlayerDirection::OUT;
                if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) playerDirection = PlayerDirection::IN;
            }
        }

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

        if (!gameWon && lg.playerSlice > lg.winningZone) {
            gameWon = true;
            PlaySound(winSound);
        }

        std::stringstream ss;
        ss << "Sim tick " << simTick << ", Player: (" << lg.playerSlice << ", " << lg.playerPosition << ")";
        debugText.text = ss.str();
        simTick++;
    }

    void Render() override {
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
    bool gameWon;
    int finalCountdown; // -1 disabled, num sims to final

    static bool classicControls;

    LevelTransformer transformer;
    Explosion explosion;
};

bool LevelGameState::classicControls = false;

class TitleScreenGameState : public LevelGameState
{
public:
    enum class PlayerDirection { CCW, CW, IN, OUT, NONE };

    TitleScreenGameState()
        : tick(0)
        , titleText(WHITE, { float(GetScreenWidth()/2), float(GetScreenHeight()/4) }, "Pix'in'", 100)
        , title2Text(WHITE, { float(GetScreenWidth() / 2), float(GetScreenHeight() / 4 + 120) }, "(Ludum Dare #48)", 20)
        , instructions(WHITE, { float(GetScreenWidth() / 2), float(GetScreenHeight() - 90) }, "Space to start game. 'k' during game to disable kill wall, 'p' to pause,", 20)
        , instructions2(WHITE, { float(GetScreenWidth() / 2), float(GetScreenHeight() - 60) }, "left/right/up/down arrows for counter-clockwise/clockwise/in/out.", 20)
        , level1(WHITE, { float(GetScreenWidth() / 2), float(GetScreenHeight() / 2 + 20) }, "Simple Level", 20)
        , level2(WHITE, { float(GetScreenWidth() / 2), float(GetScreenHeight() / 2 + 45) }, "Rando Maze", 20)
        , level3(WHITE, { float(GetScreenWidth() / 2), float(GetScreenHeight() / 2 + 70) }, "Rando Maze Hard (takes ~10 seconds)", 20)
        , controlsText(WHITE, { float(GetScreenWidth() / 2), float(GetScreenHeight() / 2 + 115) }, "", 20)
        , currLevel(0)
    {
        lg.loadBackgroundImage("Content/test_level_bg.png");
    }

    ~TitleScreenGameState()
    {
    }

    void Sim(float simTimeSeconds) override {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            if (currLevel != 3)
                finished = true;
            else
                classicControls = !classicControls;
        }
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            currLevel = (currLevel + 3) % 4;
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            currLevel = (currLevel + 1) % 4;
        }
        lg.playerSlice += 0.5f;
        ++tick;
    }

    void Render() override {
        const float centerX = static_cast<float>(GetScreenWidth() / 2);
        const float centerY = static_cast<float>(GetScreenHeight() / 2);
        transformer.screenCenter = Vector2{ centerX, centerY };
        transformer.slicesPerScreen = lg.slicesPerScreen;
        transformer.sliceAtCenter = lg.playerSlice + lg.slicesBeforePlayer;

        level1.color = GRAY;
        level2.color = GRAY;
        level3.color = GRAY;
        controlsText.color = GRAY;
        if (currLevel == 0) {
            level1.color = GREEN;
        }
        else if (currLevel == 1) {
            level2.color = GREEN;
        }
        else if (currLevel == 2) {
            level3.color = GREEN;
        }
        else {
            controlsText.color = GREEN;
        }


        lg.transformer = transformer;
        explosion.transform = transformer;

        BeginDrawing();
        ClearBackground(BLACK);
        lg.drawBackground();
        titleText.render();
        title2Text.render();
        instructions.render();
        instructions2.render();
        level1.render();
        level2.render();
        level3.render();
        if (classicControls) {
            controlsText.text = "Controls: Classic";
        }
        else {
            controlsText.text = "Controls: Direct";
        }
        controlsText.render();
        EndDrawing();
    }

    int tick;
    Text titleText;
    Text title2Text;
    Text instructions;
    Text instructions2;
    Text controlsText;

    Text level1;
    Text level2;
    Text level3;
    int currLevel;
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

    while (!IsAudioDeviceReady()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    SetMasterVolume(0.5);

    // Main game loop
    {
        bool inTitleScreen = true;
        std::unique_ptr<GameState> gameState(new TitleScreenGameState);
        while (!WindowShouldClose())    // Detect window close button or ESC key
        {
            if (gameState->finished) {
                if (inTitleScreen) {
                    int level = 0;
                    {
                        auto gs = dynamic_cast<TitleScreenGameState*>(gameState.get());
                        if (gs) level = gs->currLevel;
                    }
                    gameState.reset(new LevelGameState(level));
                }
                else {
                    gameState.reset(new TitleScreenGameState);
                }
                inTitleScreen = !inTitleScreen;
            }
            gameState->Sim(simTimeSeconds);
            gameState->Render();
        }
    }

    CloseWindow();        // Close window and OpenGL context
    CloseAudioDevice();
    return 0;
}


