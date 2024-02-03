#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<math.h>
#define FLAG_TRUE 1
#define FLAG_FALSE 0

/************************************************** シミュレーション設定 **************************************************/

/* サーバーの状態行列, パケットの構造体, クリーク行列の探索に用いる行列の成分といった途中経過を表示するモード */
#define DebugMode FLAG_TRUE

/* シード値 */
#define Seed 2

/************************************************** 条件 **************************************************/
/* サンプル数 */
#define Sample 100

/* サーバーが送信するパケット数 P_0, P_1, ..., P_(n-1), 状態行列の列column に相当する */
#define N 50

/* 受信局の数 R_0, R_1, ..., R_(M-1) 状態行列の行row に相当する */
#define M 50

/* ウインドウサイズL */
#define WindowSize 10

/* 再送パケットの上限(R_max) */
#define RetransmitLimit 100

/* ｢-1｣の成分の置換について, 0:｢0｣(受信失敗とみなす), 1:｢1｣(受信成功とみなす), -1:｢-1｣の成分を含む列はクリーク行列の探索から除外する */
#define Proposal -1

/* Proposal = 0で, 受信失敗とみなした際に, 再送したパケットのクリーク行列の探索から除外する時間κ(カッパ) */
#define Kappa 2

/* フィードバック遅延をランダムまたは一定値に変更する */
/* -1:指数乱数, それ以外:遅延時間dを入力値に固定 */
#define ConstantDelay 2

/* 指数乱数の平均 */
#define ExponentialAverage 3

/* パケット送信の失敗する確率・誤り確率。 発表スライドのε(イプシロン)がこれに該当する*/
double ErasureProbability = 0.01;

/* サーバーが1個のパケットを送信する所要時間を1単位時間とする */
long int t = 0;

/* ([行][列]で表現する)サーバーが保持する状態行列 */
int ServerStatusMatrix[M][N];

/* ([行][列]で表現する)クリーク行列 */
int CliqueMatrix[M][N];

/* 再送するパケットの番号 */
int RetransmittedPacketNumber[N];

/* (再送を含めて)送信したパケット数の合計、スループットの分母 */
long int TotalTrasmittedPacket = 0;

/* 再送回数をカウント */
int RetransmitCount = 0;

/***********************************************************************************************************************/

typedef struct Packet {
    int Status[M];   /* 受信成功(1)･失敗(0)･未受信(-1)といったパケットの状態を保存する. */
    int Delay[M];    /* フィードバックの到着するのに必要な所要(遅延)時間 */
    int FirstTime;   /* サーバーが最初に送信した時刻 */
    int ReceiveSuccessTime[M];  /* 受信成功時刻 */
    int FeedbackArrivalTime[M];  /* フィードバック到着時刻 */
    int Ignore; /* クリーク行列の探索の候補から除外する時間 */
}Packet;

/* 一様乱数 */
double Uniform(void) {
    return ((double)rand() / ((double)RAND_MAX + 1.0));
}

/* 指数乱数 */
double Exponential(void) {
    return ((-log(1 - Uniform())) * ExponentialAverage);
}

/* R_0, R_1, R_2, ..., R_(M-1) の受信局にパケットを送信する関数 */
Packet PacketTransmit(struct Packet P) {

    for (int r_i = 0; r_i < M; r_i++) {

        /* 受信局によるパケットの受信が、失敗したか成功したかを乱数で決定 */
        if (Uniform() <= ErasureProbability) {
            P.Status[r_i] = 0; /* 受信失敗 */
        }
        else {
            P.Status[r_i] = 1; /* 受信成功 */
        }

        /* 最初に送信した時刻の設定 */
        if (P.FirstTime == -1) {
            P.FirstTime = t;
        }

        /* フィードバック到着時刻の設定 */
        if (ConstantDelay == -1) {
            P.Delay[r_i] = (int)(Exponential() + 1); /* 遅延時間を指数乱数で設定する */
        }
        else {
            P.Delay[r_i] = ConstantDelay; /* 遅延時間を固定値で設定する */
        }
        P.FeedbackArrivalTime[r_i] = t + P.Delay[r_i] + 1;
        // 現時刻"t" + (フィードバックの所要･遅延時間)指数乱数 + サーバーが送信するのに必要な時間(単位時間)
    }

    /* 送信したパケットの総数を増やす */
    TotalTrasmittedPacket = TotalTrasmittedPacket + 1;

    return P;
}


/* 受信局のパケット行列、変数を初期化 */
Packet InitializationPacket(Packet P) {

    /* パケット固有の情報(｢サーバーが最初に送信した時刻｣,｢再送した時刻｣の2つ)を-1に初期化する */
    P.FirstTime = -1;

    /* 受信局ごとの情報を-1に初期化する */
    for (int r_i = 0; r_i < M; r_i++) {
        P.Status[r_i] = -1;
        P.Delay[r_i] = -1;
        P.FeedbackArrivalTime[r_i] = -1;
        P.ReceiveSuccessTime[r_i] = -1;
        P.Ignore = 0;
    }

    return P;
}

/* i番目の受信局P(r_i)のパケット行列を表示 */
void PrintPacket(struct Packet P) {

    double total = 0;
    printf("一回目の送信:%d\n", P.FirstTime);
    printf("受信成功時刻 (時間)\n");
    for (int r_i = 0; r_i < M; r_i++) {
        total += P.ReceiveSuccessTime[r_i] - P.FirstTime;
        printf("R[%d]: %d (%d)\n", r_i, P.ReceiveSuccessTime[r_i], P.ReceiveSuccessTime[r_i] - P.FirstTime);
    }
    printf("平均時間:%lf\n\n", total / M);

}
/* M行N列の行列の要素をすべて、引数xに値を変更する関数(状態行列やクリーク行列の初期化に使用する) */
void InitializationMatrix(int x, int A[M][N]) {
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            A[i][j] = x;
        }
    }
}

/* サーバーの状態行列を表示する関数 */
void PrintServerStatusMatrix(void) {
    for (int r_i = 0; r_i < M; r_i++) {
        for (int p_i = 0; p_i < N; p_i++) {
            printf(" %d ", ServerStatusMatrix[r_i][p_i]);
        }
        printf("\n");
    }
    printf("\n");
}

/* サーバーの状態行列と,パケットの構造体にある受信完了時刻を更新する関数 */
Packet ServerMatrixUpdate(int PacketNumber, Packet P) {
    for (int r_i = 0; r_i < M; r_i++) {
        if (t == P.FeedbackArrivalTime[r_i] && P.ReceiveSuccessTime[r_i] == -1) { /* 未到着のフィードバックが現時刻になってサーバーに届いた場合 */
            /* フィードバックが到着したi番目の受信局(r_i)ごとにサーバーの状態行列へ保存する */
            ServerStatusMatrix[r_i][PacketNumber] = P.Status[r_i];
        }
        if (ServerStatusMatrix[r_i][PacketNumber] == 1) { /* フィードバックによって、受信局がパケットの受信に成功したと判明した場合 */

            /* まだ受信成功時間が保存されていない状態の場合 */
            if (P.ReceiveSuccessTime[r_i] == -1) {
                /* 受信成功時間を保存する */
                P.ReceiveSuccessTime[r_i] = t;
            }
        }
    }

    return P;
}


Packet PacketRetransmitting(int PacketNumber, Packet P) {

    if (P.FirstTime == -1) {
        return P;
    }

    for (int r_i = 0; r_i < M; r_i++) {
        if (P.Status[r_i] == 0) {      /* 受信局がサーバーから送られたパケットの受信に失敗していた場合。既に受信成功している受信局には、パケットの再送はしないと仮定 */

            /* 成功・失敗の判定:パケットの再送に相当する */
            if (Uniform() <= ErasureProbability) {
                P.Status[r_i] = 0; /* 受信失敗 */
            }
            else {
                P.Status[r_i] = 1; /* 受信成功 */
            }

            /* 再送したパケットの対応する、サーバーの状態行列にある要素を-1(フィードバック未到着)に変更する */
            ServerStatusMatrix[r_i][PacketNumber] = -1;

            /* フィードバック到着時刻の設定 */
            if (ConstantDelay == -1) {
                P.Delay[r_i] = (int)(Exponential() + 1); /* 指数乱数で設定する */
            }
            else {
                P.Delay[r_i] = ConstantDelay; /* 指定した固定値で設定する */
            }
            P.FeedbackArrivalTime[r_i] = t + P.Delay[r_i] + 1;

        }
    }

    /* フィードバック未到着のパケットを受信に失敗していると仮定した手法をとっている場合 */
    if (Proposal == 0) {
        P.Ignore = Kappa; /* 指定した固定値κの単位時間分, クリーク行列の探索の候補に入れない. */
    }

    return P;
}

void Clique_Search(void) {

    int r_i, p_i;

    /* 状態行列の中からランダムに選択した列の番号 */
    int RandomColumnNumber;

    /* 試行回数のカウント */
    int count = 0;

    /* クリーク行列で0の成分の行番号 */
    int CliqueZeroRowNumber[M];
    int CliqueRowNode = 0;

    /* 探索した(ランダムに選択した)列に含まれている0の行番号 */
    int SelectedColumnNumber[M];

    int InOrder = 0;

    /* 0の要素・成分がある行番号を保存する配列の初期化, すべてを終端記号の-1にする */
    for (int i = 0; i < M; i++) {
        CliqueZeroRowNumber[i] = -1;
        SelectedColumnNumber[i] = -1;
    }

    /* 再送するパケット番号を格納するint型の配列を初期化する */
    for (int i = 0; i < N; i++) {
        RetransmittedPacketNumber[i] = -1;
    }
    int RetransmitNode = 0;

    if (Proposal != -1) {
        /* 探索するためにサーバーの状態行列をコピーする */
        for (r_i = 0; r_i < M; r_i++) {
            for (p_i = 0; p_i < N; p_i++) {

                /* 状態行列のp_i番目の列が、すべて-1ならクリーク行列にする際の置換は行わない */
                int ThisColumnisAllMinus1 = FLAG_FALSE;
                for (int i = 0; i < M; i++) {
                    if (ServerStatusMatrix[i][p_i] == -1) {
                        if (i == (M - 1)) {
                            ThisColumnisAllMinus1 = FLAG_TRUE;
                            break;
                        }
                    }
                    else {
                        ThisColumnisAllMinus1 = FLAG_FALSE;
                        break;
                    }

                }

                /* 「すべて-1」ではなく、部分的に-1の値を取っている場合は、成分の置換を行う. */
                if (ThisColumnisAllMinus1 == FLAG_FALSE && ServerStatusMatrix[r_i][p_i] == -1) {
                    CliqueMatrix[r_i][p_i] = Proposal;
                }
                else {
                    CliqueMatrix[r_i][p_i] = ServerStatusMatrix[r_i][p_i];
                }
            }
        }
        if (DebugMode == 1) {
            printf("\n################################################################################\n");
            printf("クリーク行列の探索を行う行列:\n");
            for (r_i = 0; r_i < M; r_i++) {
                for (p_i = 0; p_i < N; p_i++) {
                    printf(" %d ", CliqueMatrix[r_i][p_i]);
                }
                printf("\n");
            }
            printf("################################################################################\n\n\n\n");
        }
    }
    else if (Proposal == -1) {
        for (r_i = 0; r_i < M; r_i++) {
            for (p_i = 0; p_i < N; p_i++) {
                CliqueMatrix[r_i][p_i] = ServerStatusMatrix[r_i][p_i];
            }
        }
    }

    while (1) {
        if (count > N) {
            /* ランダムに列を選択する回数が指定した数字を超えた場合 */
            /* 0～N-1の数字の並びをランダムに入れ替えて(1個のランダムな順列を作成する)，選択漏れがないようにする */
            /* 1回ごとにランダムに列を一つ選択するのではない */

            if (DebugMode == 1) {
                if (RetransmittedPacketNumber[0] == -1) {
                    printf("再送しませんでした.\n");
                }
                else {
                    printf("再送するパケット番号:");
                    for (int i = 0; i < N; i++) {
                        if (RetransmittedPacketNumber[i] != -1) {
                            printf("%d, ", RetransmittedPacketNumber[i]);
                        }
                        else {
                            break;
                        }
                    }
                    printf("\n");
                }
            }

            /* 再送及び合成するパケット番号を保存・出力してクリーク行列の探索を終了する */
            break;
        }

        for (int i = 0; i < M; i++) {
            SelectedColumnNumber[i] = -1;
        }

        /* まずはパケット番号順にクリーク行列の探索をする場合 */

        RandomColumnNumber = InOrder;
        InOrder = InOrder + 1;
        if (InOrder >= N + 1) {
            RandomColumnNumber = (int)((Uniform()) * N);
        }


        //RandomColumnNumber = (Uniform()) * N;

        /* RandomColumnNumberの列番号の(ランダムに選択した列)列に含まれている0の行番号を保存する配列を初期化する */

        /* ランダムに選択した列に含まれている0の行番号をSelectedColumnNumberに格納する */
        /* 例:0行目,2行目,3行目が0ならば,SelectedColumnNumber = {0, 2, 3, -1, -1...}となる */
        int ColumnNode = 0;
        for (int r_i = 0; r_i < M; r_i++) {
            if (CliqueMatrix[r_i][RandomColumnNumber] == 0) {
                SelectedColumnNumber[ColumnNode] = r_i;
                ColumnNode = ColumnNode + 1;
            }
        }

        /* 既にクリーク行列に0になっている行と重複しないかチェックする */
        int RowFlag = FLAG_TRUE; /* TRUE:クリーク行列に加える FALSE:クリーク行列に加えない */

        /* 選択した列が、クリーク行列に入らないかチェックする */
        /* 0の要素の行番号が重複している場合は除外 */
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {
                if ((SelectedColumnNumber[i] != -1) && (CliqueZeroRowNumber[j] != -1)) { /* ランダムに選択した列とクリーク行列にある0の要素の行番号を比較する */
                    if (SelectedColumnNumber[i] == CliqueZeroRowNumber[j]) {/* 既にクリーク行列にある列で、同じ行番号に0の要素がある場合 */
                        RowFlag = FLAG_FALSE; /* ランダムに選択した列をクリーク行列に加えないことにする */
                        /* for文から抜ける */
                        i = M;
                        j = M;
                        break;
                    }
                }
            }
        }

        /* -1を含んでいる場合は除外 */
        for (int i = 0; i < M; i++) {
            if (CliqueMatrix[i][RandomColumnNumber] == -1) {
                RowFlag = FLAG_FALSE;
                break;
            }
        }

        /* すべての要素が1なら除外 */
        for (int i = 0; i < M; i++) {
            if (CliqueMatrix[i][RandomColumnNumber] == 1) {
                if (i == (M - 1)) {/* 選択した列の最後の行まで要素がずっと1であることを確認したら */
                    /* ランダムに選択した列をクリーク行列に加えないようにする */
                    RowFlag = FLAG_FALSE;
                }
            }
            else if (CliqueMatrix[i][RandomColumnNumber] == 0) { /* 要素が0の行が存在することが確認出来たら */
                /* ランダムに選択した列をクリーク行列に加えることに決定する */
                break;
            }
        }

        /* クリーク行列に含まれている0の行番号を格納した配列を処理する。その配列にランダムに選択した列の0の要素の行番号を追加する */
        if (RowFlag == FLAG_TRUE) {
            int ColumnNode = 0;
            for (int i = 0; i < M; i++) {
                if (CliqueZeroRowNumber[i] == -1) {
                    CliqueZeroRowNumber[i] = SelectedColumnNumber[ColumnNode];
                    ColumnNode = ColumnNode + 1;
                }
            }
        }

        /* 既に追加されている場合は追加をしない */
        for (int i = 0; i < N; i++) {
            if (RetransmittedPacketNumber[i] == RandomColumnNumber) {
                RowFlag = FLAG_FALSE;
                break;
            }
        }

        /* 再送・合成するパケット番号に追加する */
        if (RowFlag == FLAG_TRUE) {
            RetransmittedPacketNumber[RetransmitNode] = RandomColumnNumber;
            RetransmitNode = RetransmitNode + 1;
        }

        /* 試行回数を増やす */
        count = count + 1;
    }

}

void TimePassed(void) {
    if (DebugMode == 1) {
        printf("t:%ld\n", t + 1);
    }
    t = t + 1;
}

/***********************************************************************************************************************/
int main(void) {

    /* i番目の受信局 */
    int r_i;

    /* i番目のパケット */
    int p_i;

    /* スループット */
    double Throughput = 0.0;            //一サンプルあたりの結果
    double ResultThroughput = 0.0;      //すべてのサンプルの結果を平均した値(シミュレーション結果)

    /* 所要時間の合計 */
    int TotalTime = 0;

    /* 平均到着時間 */
    double AverageArrivalTime = 0.0;    //一サンプルあたりの結果
    double ResultArrival = 0.0;         //すべてのサンプルの結果を平均した値(シミュレーション結果)

    /* 現時刻で一回目の送信をしているウインドウの番号 */
    int WindowNumber = 0;

    /* 最後のウインドウを判定するフラグ */
    int FinalWindowFlag = 0;

    /* N個のパケット */
    Packet P[N];

    /* シード値から乱数を生成 */
    srand((unsigned)Seed);

    /* 現在実行中のサンプル番号. 指定したサンプル数に達したらシミュレーションを終了する. */
    int SampleNum = 0;

    /* シミュレーションの準備 */
    if (WindowSize > N) {
        printf("ウインドウのサイズ(%d)がパケット数N(%d)を超えているため，分割できません. シミュレーションを終了します.\n", WindowSize, N);
        return -1;
    }
    else if (N % WindowSize != 0) {
        printf("パケット数N(%d)に対して,指定したウインドウサイズ(%d)で分割できません. シミュレーションを終了します.\n", N, WindowSize);
        return -1;
    }
    
    while (1) {

        /* 消失確率εが0.10を超えた場合, シミュレーションを終了する */
        if (ErasureProbability > 0.10) {
            break;
        }
        SampleNum = 0;
        ResultArrival = 0.0;
        ResultThroughput = 0.0;

        /* 消失確率εを固定して, 指定したサンプル数だけ繰り返し実験を行う */
        while (1) {
            t = 0;
            WindowNumber = 0;
            FinalWindowFlag = 0;
            TotalTrasmittedPacket = 0;

            if (SampleNum == Sample) {
                break;
            }

            /* サーバーに保持される状態行列をすべて-1に初期化 */
            InitializationMatrix(-1, ServerStatusMatrix);

            /* パケットの構造体を初期化 */
            for (int i = 0; i < N; i++) {
                P[i] = InitializationPacket(P[i]);
            }

            while (1) {
                /* サーバーの状態行列がすべて1になったらシミュレーションを終了 */
                int SimulationFlag = FLAG_TRUE; //1:すべて1, 0:すべて1ではなく,0が含まれている
                for (r_i = 0; r_i < M; r_i++) {
                    for (p_i = 0; p_i < N; p_i++) {
                        if (ServerStatusMatrix[r_i][p_i] == 0 || ServerStatusMatrix[r_i][p_i] == -1) {
                            SimulationFlag = FLAG_FALSE;
                            r_i = M;
                            p_i = N;
                            break;
                        }
                    }
                }

                if (SimulationFlag == FLAG_TRUE) {
                    break;
                }

                /* サーバーの状態行列を更新 */
                for (p_i = 0; p_i < N; p_i++) {
                    P[p_i] = ServerMatrixUpdate(p_i, P[p_i]);
                    /* パケットの構造体で受信成功時刻を更新 */
                }

                /* 現在(WindowNumber)のウインドウにあるすべてのパケットを一度送信する */
                for (int FirstSendingNumber = WindowSize * WindowNumber; FirstSendingNumber < WindowSize * (WindowNumber + 1); FirstSendingNumber = FirstSendingNumber + 1) {

                    /* 次のウインドウがない場合は一回目のパケット送信を終了する */
                    if (FinalWindowFlag == FLAG_TRUE) {
                        /* FinalWindowFlag  TRUE:現在のウインドウが最後のウインドウ,  FALSE:最後のウインドウではない */
                        break;
                    }

                    /* パケットの一回目の送信 */
                    P[FirstSendingNumber] = PacketTransmit(P[FirstSendingNumber]);

                    /* 時間経過 */
                    TimePassed();

                    /* サーバーの状態行列と、パケットの受信成功時刻を更新する */
                    for (p_i = 0; p_i < N; p_i++) {
                        P[p_i] = ServerMatrixUpdate(p_i, P[p_i]);
                    }

                    if (DebugMode == 1) {
                        /* サーバーの状態行列を表示 */
                        PrintServerStatusMatrix();
                    }
                }

                int WindowFlag; /* TRUE:次のウインドウに移行する, FALSE:次のウインドウに移行しない */

                /* 現在(WindowNumber)のウインドウにある受信失敗したパケットを再送するまでループ:フラグWindowFlag */
                while (1) {
                    WindowFlag = FLAG_TRUE;
                    /* Windowに限らず、すべてのパケットの状態を保存したサーバーの状態行列からクリーク行列の探索 */
                    Clique_Search();

                    /* クリーク行列で抽出した列番号のパケットを再送 */
                    for (int i = 0; i < N; i++) {
                        if (RetransmittedPacketNumber[i] != -1) {
                            P[RetransmittedPacketNumber[i]] = PacketRetransmitting(RetransmittedPacketNumber[i], P[RetransmittedPacketNumber[i]]);
                            WindowFlag = FLAG_FALSE;
                        }
                        else if (RetransmittedPacketNumber[i] == -1) {
                            break;
                        }
                    }

                    /* 再送をしたら1単位時間経過させる */
                    if (WindowFlag == FLAG_FALSE) {
                        TimePassed();
                        for (int i = 0; i < N; i++) {
                            P[i] = ServerMatrixUpdate(i, P[i]);
                        }
                        if (DebugMode == 1) {
                            PrintServerStatusMatrix();
                        }
                        /* 再送したため、送信したパケットの総数と再送回数をインクリメントする */
                        TotalTrasmittedPacket = TotalTrasmittedPacket + 1;
                        RetransmitCount = RetransmitCount + 1;
                    }

                    /* サーバーの状態行列を更新する(再送及び時間経過していない場合は変化なし) */
                    for (int i = 0; i < N; i++) {
                        P[i] = ServerMatrixUpdate(i, P[i]);
                    }

                    /* ↓再送するパケットがなかったら次のウインドウに移行している */
                    if (WindowFlag == FLAG_TRUE) {
                        WindowFlag = FLAG_FALSE;
                        break;
                    }
                }/* 現在のウインドウにあるパケットが受信完了するまでwhile文でループする。その処理の終端 */

                /* 次のWindowに移行する */
                if ((WindowNumber + 1) < (N / WindowSize)) {
                    WindowNumber = WindowNumber + 1;
                }
                else {

                    /* 最後のウインドウの場合は、時間経過させてサーバーの状態行列を更新する */
                    TimePassed();
                    FinalWindowFlag = 1;
                    /* サーバーの状態行列を更新する(再送及び時間経過していない場合は変化なし) */
                    for (int i = 0; i < N; i++) {
                        P[i] = ServerMatrixUpdate(i, P[i]);
                    }

                    if (DebugMode == 1) {
                        PrintServerStatusMatrix();
                    }
                }
            }

            /* パケットの平均到着時間の計算 */
            TotalTime = 0;
            for (p_i = 0; p_i < N; p_i++) {
                for (r_i = 0; r_i < M; r_i++) {
                    TotalTime = TotalTime + (P[p_i].ReceiveSuccessTime[r_i] - P[p_i].FirstTime);
                }
            }

            /* パケットが受信成功する際の平均所要時間 */
            AverageArrivalTime = (double)TotalTime / (double)N;
            AverageArrivalTime = (double)AverageArrivalTime / (double)M;
            ResultArrival = ResultArrival + AverageArrivalTime;

            /* スループット */
            Throughput = (double)N / (double)TotalTrasmittedPacket;
            ResultThroughput = ResultThroughput + Throughput;

            /* サンプル番号をインクリメント */
            SampleNum = SampleNum + 1;
        }

        ResultArrival = ResultArrival / (double)Sample;
        ResultThroughput = ResultThroughput / (double)Sample;

        if (DebugMode == FLAG_TRUE) {
            for (int i = 0; i < N; i++) {
                printf("P_%d:\n", i);
                PrintPacket(P[i]);
            }
            /* シミュレーションで実行した手法の表示 */
            if (Proposal == 0) {
                printf("｢-1｣を｢0｣と置換する.\n");
            }
            else if (Proposal == 1) {
                printf("｢-1｣を｢1｣と置換する\n");
            }
            else if (Proposal == -1) {
                printf("｢-1｣の成分を含む列をクリーク行列の探索から除外する\n");
            }

            /* フィードバック遅延の設定を表示 */
            if (ConstantDelay == -1) {
                printf("フィードバック遅延時間:指数乱数\n");
            }
            else {
                printf("フィードバック遅延時間(固定):%d\n", ConstantDelay);
            }

            /* シミュレーション条件の表示 */
            printf("サンプル数:%d\n", Sample);
            printf("パケットの数N:%d, 受信局の数M:%d\n", N, M);
            printf("誤り確率:%.2lf\n", ErasureProbability);
            printf("ウインドウサイズL:%d\n", WindowSize);
            printf("再送上限:%d\n", RetransmitLimit);

            /* シミュレーション結果の表示 */
            printf("パケットが受信成功する際の平均所要時間:%lf\n", ResultArrival);
            printf("スループット:%lf\n", ResultThroughput);
        }
        else {
            //printf("N:%d, M:%d, L:%d\n", N, M, WindowSize);
            printf("%.2lf \t %lf \t %lf\n", ErasureProbability, ResultArrival, ResultThroughput);
        }
          
        ErasureProbability = ErasureProbability + 0.01;
    }

    return 0;
}//main関数


