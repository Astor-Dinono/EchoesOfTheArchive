#pragma once

namespace ArchiveRules
{
    constexpr int Size = 17;
    constexpr int CoinCount = 60;
    constexpr int ShardCount = 6;
    constexpr int CoinScore = 10;
    constexpr int DecoyThreshold = 200;
    constexpr int PulseThreshold = 400;
    constexpr int DecoyUses = 3;
    constexpr int PulseUses = 2;
    constexpr float PulseDuration = 3.f;
    constexpr float PulseCooldown = 20.f;
    constexpr float PulseRange = 800.f;
    constexpr float CellSize = 400.f;
    struct Cell
    {
        int X, Y;
        bool operator==(const Cell& Other) const { return X == Other.X && Y == Other.Y; }
        bool operator!=(const Cell& Other) const { return !(*this == Other); }
    };
    static const char* const Layout[Size] = {
        "#################",
        "#S....#.........#",
        "#.###.#.###.###.#",
        "#...#...#...#...#",
        "###.#.###.#.#.#.#",
        "#...#.....#...#.#",
        "#.#####.#####.#.#",
        "#.....G...#.....#",
        "#.###.###.#.###.#",
        "#.#.....#...#...#",
        "#.#.###.#####.#.#",
        "#...#...#.....#.#",
        "#.###.#.#.#####.#",
        "#.....#...H.....#",
        "#.#######.#.###.#",
        "#...E.....#....C#",
        "#################"
    };
    static const Cell Start = {1, 1};
    static const Cell Exit = {4, 15};
    static const Cell EnemyStart = {15, 15};
    static const Cell PowerDoor = {6, 7};
    static const Cell ShelfDoor = {10, 13};
    static const Cell Shards[ShardCount] = {{5,1}, {15,3}, {1,7}, {7,9}, {1,15}, {13,15}};

    inline bool Walkable(Cell C, bool Power, bool Shelf)
    {
        if (C.X < 0 || C.Y < 0 || C.X >= Size || C.Y >= Size) return false;
        const char V = Layout[C.Y][C.X];
        return V != '#' && (V != 'G' || Power) && (V != 'H' || Shelf);
    }
    inline int Index(Cell C) { return C.Y * Size + C.X; }
    inline bool NextStep(Cell From, Cell To, bool Power, bool Shelf, Cell& Out)
    {
        if (!Walkable(From, Power, Shelf) || !Walkable(To, Power, Shelf)) return false;
        if (From == To) { Out = From; return true; }
        int Parent[Size*Size];
        for (int& P : Parent) P = -1;
        Cell Queue[Size*Size];
        int Head = 0, Tail = 0;
        Queue[Tail++] = From;
        Parent[Index(From)] = Index(From);
        const Cell Dirs[4] = {{1,0}, {0,1}, {-1,0}, {0,-1}};
        while (Head < Tail)
        {
            const Cell C = Queue[Head++];
            for (const Cell D : Dirs)
            {
                const Cell N{C.X+D.X, C.Y+D.Y};
                if (!Walkable(N, Power, Shelf) || Parent[Index(N)] != -1) continue;
                Parent[Index(N)] = Index(C);
                if (N == To)
                {
                    int I = Index(N);
                    while (Parent[I] != Index(From)) I = Parent[I];
                    Out = {I % Size, I / Size};
                    return true;
                }
                Queue[Tail++] = N;
            }
        }
        return false;
    }
    inline bool IsShard(Cell C)
    {
        for (const Cell S : Shards) if (S == C) return true;
        return false;
    }
    inline int CoinCells(Cell* Out)
    {
        Cell Candidates[Size*Size]; int Count = 0;
        for (int Y=1; Y<Size-1; ++Y)
            for (int X=1; X<Size-1; ++X)
            {
                const Cell C{X,Y};
                if (Walkable(C,false,false) && C != Start && C != Exit && C != EnemyStart && !IsShard(C))
                    Candidates[Count++] = C;
            }
        const int N = Count < CoinCount ? Count : CoinCount;
        for (int I=0; I<N; ++I) Out[I] = Candidates[(I*Count)/N];
        return N;
    }
    struct Progress
    {
        int Score = 0, Coins = 0, ShardsHeld = 0, Decoys = 0, Pulses = 0;
        bool bDecoyUnlocked = false, bPulseUnlocked = false;
        bool bWon = false, bLost = false;
        bool TakenCoins[CoinCount] = {};
        bool TakenShards[ShardCount] = {};
        bool Active() const { return !bWon && !bLost; }
        bool CollectCoin(int Id)
        {
            if (!Active() || Id < 0 || Id >= CoinCount || TakenCoins[Id]) return false;
            TakenCoins[Id] = true; ++Coins; Score += CoinScore;
            if (!bDecoyUnlocked && Score >= DecoyThreshold) { bDecoyUnlocked = true; Decoys = DecoyUses; }
            if (!bPulseUnlocked && Score >= PulseThreshold) { bPulseUnlocked = true; Pulses = PulseUses; }
            return true;
        }
        bool CollectShard(int Id)
        {
            if (!Active() || Id < 0 || Id >= ShardCount || TakenShards[Id]) return false;
            TakenShards[Id] = true; ++ShardsHeld; return true;
        }
        bool TryEscape()
        {
            if (!Active() || ShardsHeld != ShardCount) return false;
            bWon = true; return true;
        }
    };
}
