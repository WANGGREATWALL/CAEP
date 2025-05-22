#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
using namespace std;

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

// 物理参数常量
constexpr int NDIVX = 100;       // x方向网格数
constexpr int NDIVY = 100;       // y方向网格数
constexpr int NBAND = 3;         // 边界层数
constexpr int NTOTNODE = NDIVX * (NDIVY + 2 * NBAND);  // 总粒子数
constexpr int NT = 1000;         // 总时间步
constexpr int MAXFAM = 100;      // 最大邻居数
constexpr double HOLE_RADIUS = 0.005;  // 中心孔半径

// 二维向量结构体（包含坐标和运算）
struct Vec2 {
    double x, y;
    Vec2(double x = 0, double y = 0) : x(x), y(y) {}
    Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(double scalar) const { return {x * scalar, y * scalar}; }
    double magnitude() const { return sqrt(x*x + y*y); }  // 向量模长
};

// 计算两点间距离
static double distance(const Vec2& a, const Vec2& b) {
    return (a - b).magnitude();
}

int demo_origin() {
    // 物理参数初始化
    double length = 0.05;       // 板长度
    double width = 0.05;        // 板宽度
    double dx = length / NDIVX; // 粒子间距
    double delta = 3.015 * dx;   // 作用域半径
    double thick = dx;          // 板厚度
    double dens = 8000.0;       // 密度
    double emod = 192.0e9;      // 弹性模量
    double pratio = 1.0 / 3.0;  // 泊松比
    double vol = dx * dx * thick; // 单个粒子体积
    double bc = 9.0 * emod / (M_PI * thick * pow(delta, 3)); // 键常数
    double sedload1 = 9.0/16.0 * emod * 1.0e-6; // 加载1应变能密度
    double sedload2 = 9.0/16.0 * emod * 1.0e-6; // 加载2应变能密度
    double dt = 1.0;           // 时间步长
    double scr0 = 0.02;        // 临界拉伸阈值

    // 初始化数组（使用vector动态分配）
    vector<Vec2> coord(NTOTNODE);       // 粒子坐标
    vector<Vec2> disp(NTOTNODE, {0, 0}); // 位移
    vector<Vec2> vel(NTOTNODE, {0, 0});  // 速度
    vector<Vec2> pforce(NTOTNODE, {0, 0}); // 总作用力
    vector<Vec2> massvec(NTOTNODE);      // 质量向量（ADR用）
    vector<int> numfam(NTOTNODE, 0);     // 邻居数量
    vector<int> pointfam(NTOTNODE, 0);    // 邻居索引偏移
    vector<int> nodefam(10000000, 0);     // 邻居列表（大数组预分配）
    vector<vector<int>> fail(NTOTNODE, vector<int>(MAXFAM, 1)); // 连接状态（1=有效）
    vector<double> dmg(NTOTNODE, 0.0);    // 损伤参数

    // 1. 生成粒子坐标（内部区域 + 边界区域）
    int nnum = 0; // 当前粒子编号

    // 内部区域（排除中心孔）
    for (int i = 1; i <= NDIVY; ++i) {
        for (int j = 1; j <= NDIVX; ++j) {
            double x = -length/2 + dx/2 + (j-1)*dx;
            double y = -width/2 + dx/2 + (i-1)*dx;
            if (sqrt(x*x + y*y) > HOLE_RADIUS) {
                coord[nnum] = {x, y};
                nnum++;
            }
        }
    }
    int totint = nnum; // 内部粒子数

    // 底部边界粒子（y方向外侧）
    for (int i = 1; i <= NBAND; ++i) {
        for (int j = 1; j <= NDIVX; ++j) {
            double x = -length/2 + dx/2 + (j-1)*dx;
            double y = -width/2 - dx/2 - (i-1)*dx; // 向下扩展
            coord[nnum++] = {x, y};
        }
    }
    int totbottom = nnum; // 底部边界后总粒子数

    // 顶部边界粒子（y方向外侧）
    for (int i = 1; i <= NBAND; ++i) {
        for (int j = 1; j <= NDIVX; ++j) {
            double x = -length/2 + dx/2 + (j-1)*dx;
            double y = width/2 + dx/2 + (i-1)*dx; // 向上扩展
            coord[nnum++] = {x, y};
        }
    }
    int tottop = nnum; // 顶部边界后总粒子数

    // 2. 邻域搜索：建立每个粒子的邻居列表
    for (int i = 0; i < NTOTNODE; ++i) {
        if (i > 0) {
            pointfam[i] = pointfam[i-1] + numfam[i-1]; // 计算邻居索引偏移
        }
        for (int j = 0; j < NTOTNODE; ++j) {
            if (i != j) {
                double dist = distance(coord[i], coord[j]);
                if (dist <= delta) {
                    nodefam[pointfam[i] + numfam[i]] = j; // 存储邻居索引
                    numfam[i]++; // 邻居数+1
                }
            }
        }
    }

    // 3. 计算表面修正因子（加载1：x方向）
    for (int i = 0; i < NTOTNODE; ++i) {
        disp[i].x = 0.001 * coord[i].x; // 初始加载位移
        disp[i].y = 0.0;
    }
    vector<double> stendens_x(NTOTNODE, 0.0); // 加载1的应变能密度
    for (int i = 0; i < NTOTNODE; ++i) {
        for (int j = 0; j < numfam[i]; ++j) {
            int cnode = nodefam[pointfam[i] + j];
            double idist = distance(coord[i], coord[cnode]);
            Vec2 disp_ij = (coord[cnode] + Vec2(disp[cnode].x, disp[cnode].y)) - (coord[i] + Vec2(disp[i].x, disp[i].y));
            double nlength = disp_ij.magnitude();
            double fac = (idist <= delta - dx/2) ? 1.0 : 
                        (idist <= delta + dx/2) ? (delta + dx/2 - idist) / dx : 0.0;
            stendens_x[i] += 0.25 * bc * pow((nlength - idist)/idist, 2) * idist * vol * fac;
        }
        if (stendens_x[i] != 0) {
            disp[i].x = 0.0; // 重置位移，仅用于计算修正因子
        }
    }
    vector<double> fncst_x(NTOTNODE, 0.0); // x方向修正因子
    for (int i = 0; i < NTOTNODE; ++i) {
        fncst_x[i] = sedload1 / stendens_x[i];
    }

    // 4. 计算表面修正因子（加载2：y方向）
    for (int i = 0; i < NTOTNODE; ++i) {
        disp[i].y = 0.001 * coord[i].y;
        disp[i].x = 0.0;
    }
    vector<double> stendens_y(NTOTNODE, 0.0); // 加载2的应变能密度
    for (int i = 0; i < NTOTNODE; ++i) {
        for (int j = 0; j < numfam[i]; ++j) {
            int cnode = nodefam[pointfam[i] + j];
            double idist = distance(coord[i], coord[cnode]);
            Vec2 disp_ij = (coord[cnode] + Vec2(disp[cnode].x, disp[cnode].y)) - (coord[i] + Vec2(disp[i].x, disp[i].y));
            double nlength = disp_ij.magnitude();
            double fac = (idist <= delta - dx/2) ? 1.0 : 
                        (idist <= delta + dx/2) ? (delta + dx/2 - idist) / dx : 0.0;
            stendens_y[i] += 0.25 * bc * pow((nlength - idist)/idist, 2) * idist * vol * fac;
        }
        if (stendens_y[i] != 0) {
            disp[i].y = 0.0; // 重置位移
        }
    }
    vector<double> fncst_y(NTOTNODE, 0.0); // y方向修正因子
    for (int i = 0; i < NTOTNODE; ++i) {
        fncst_y[i] = sedload2 / stendens_y[i];
    }

    // 5. 初始化质量向量（用于自适应动态松弛算法）
    for (int i = 0; i < NTOTNODE; ++i) {
        massvec[i] = {
            0.25 * dt * dt * M_PI * pow(delta, 2) * thick * bc / dx,
            0.25 * dt * dt * M_PI * pow(delta, 2) * thick * bc / dx
        };
    }

    // 6. 时间积分主循环
    vector<Vec2> velhalfold(NTOTNODE, {0, 0}); // 前半步速度
    vector<Vec2> pforceold(NTOTNODE, {0, 0});  // 前一时间步力

    for (int tt = 1; tt <= NT; ++tt) {
        double ctime = tt * dt;
        cout << "Time step: " << tt << endl;

        // --------------------- 边界条件 ---------------------
        // 底部边界（固定速度向下）
        for (int i = totint; i < totbottom; ++i) {
            vel[i].y = -2.7541e-7;
            disp[i].y = vel[i].y * ctime;
        }
        // 顶部边界（固定速度向上）
        for (int i = totbottom; i < tottop; ++i) {
            vel[i].y = 2.7541e-7;
            disp[i].y = vel[i].y * ctime;
        }

        // --------------------- 力计算与损伤评估 ---------------------
        fill(pforce.begin(), pforce.end(), Vec2{0, 0}); // 重置力
        fill(dmg.begin(), dmg.end(), 0.0); // 重置损伤

        for (int i = 0; i < totint; ++i) { // 仅内部粒子参与断裂计算
            double dmgpar1 = 0.0, dmgpar2 = 0.0;
            for (int j = 0; j < numfam[i]; ++j) {
                int cnode = nodefam[pointfam[i] + j];
                Vec2 r_ij = coord[cnode] - coord[i];          // 初始相对位置
                Vec2 u_ij = (coord[cnode] + disp[cnode]) - (coord[i] + disp[i]); // 变形后相对位置
                double idist = r_ij.magnitude();              // 初始距离
                double nlength = u_ij.magnitude();            // 变形后距离
                double stretch = (nlength - idist) / idist;    // 拉伸量

                // 体积修正因子（处理边界效应）
                double fac = 0.0;
                if (idist <= delta - dx/2) {
                    fac = 1.0;
                } else if (idist <= delta + dx/2) {
                    fac = (delta + dx/2 - idist) / dx;
                }

                // 角度计算（用于各向异性修正）
                double theta = 0.0;
                if (idist > 1e-10) {
                    double dx_ij = abs(r_ij.x), dy_ij = abs(r_ij.y);
                    theta = atan2(dy_ij, dx_ij); // 0到π/2之间的角度
                }

                // 表面修正因子（考虑不同方向的材料特性差异）
                double scx = (fncst_x[i] + fncst_x[cnode]) / 2.0;
                double scy = (fncst_y[i] + fncst_y[cnode]) / 2.0;
                double scr = 1.0 / sqrt(pow(cos(theta)/scx, 2) + pow(sin(theta)/scy, 2));

                // 计算PD力（基于状态的Peridynamics模型）
                if (fail[i][j] == 1) { // 连接有效时计算力
                    if (nlength > 1e-10) { // 避免零除
                        Vec2 force = u_ij * (bc * stretch * vol * scr * fac / nlength);
                        pforce[i] = pforce[i] + force;
                    }
                }

                // 判断是否断裂（临界拉伸+区域限制）
                if (abs(stretch) > scr0 && abs(coord[i].y) <= length/4.0) {
                    fail[i][j] = 0; // 标记为断裂
                }

                // 损伤参数累加（统计有效连接比例）
                dmgpar1 += fail[i][j] * vol * fac;
                dmgpar2 += vol * fac;
            }
            if (dmgpar2 > 1e-10) {
                dmg[i] = 1.0 - dmgpar1 / dmgpar2; // 计算损伤度（0=无损，1=完全断裂）
            }
        }

        // --------------------- 自适应动态松弛（ADR）算法 ---------------------
        double cn = 0.0, cn1 = 0.0, cn2 = 0.0;
        for (int i = 0; i < totint; ++i) {
            if (velhalfold[i].x != 0.0) {
                double acc_diff = (pforce[i].x - pforceold[i].x) / massvec[i].x;
                cn1 -= disp[i].x * disp[i].x * acc_diff / (dt * velhalfold[i].x);
            }
            if (velhalfold[i].y != 0.0) {
                double acc_diff = (pforce[i].y - pforceold[i].y) / massvec[i].y;
                cn1 -= disp[i].y * disp[i].y * acc_diff / (dt * velhalfold[i].y);
            }
            cn2 += disp[i].x * disp[i].x + disp[i].y * disp[i].y;
        }

        if (cn2 > 1e-10) {
            cn = (cn1 / cn2 > 0.0) ? 2.0 * sqrt(cn1 / cn2) : 0.0;
        }
        cn = min(cn, 1.9); // 限制最大松弛系数

        // --------------------- 速度和位移更新（显式积分） ---------------------
        for (int i = 0; i < totint; ++i) {
            Vec2 velhalf;
            if (tt == 1) { // 初始时间步特殊处理
                velhalf.x = dt * (pforce[i].x) / (2 * massvec[i].x);
                velhalf.y = dt * (pforce[i].y) / (2 * massvec[i].y);
            } else {
                // ADR算法更新半时间步速度
                velhalf.x = ((2.0 - cn * dt) * velhalfold[i].x + 2.0 * dt * pforce[i].x / massvec[i].x) / (2.0 + cn * dt);
                velhalf.y = ((2.0 - cn * dt) * velhalfold[i].y + 2.0 * dt * pforce[i].y / massvec[i].y) / (2.0 + cn * dt);
            }

            // 更新全时间步速度和位移
            vel[i] = { (velhalfold[i].x + velhalf.x) * 0.5, (velhalfold[i].y + velhalf.y) * 0.5 };
            disp[i] = { disp[i].x + velhalf.x * dt, disp[i].y + velhalf.y * dt };

            // 保存半步速度和前步力（用于下一步计算）
            velhalfold[i] = velhalf;
            pforceold[i] = pforce[i];
        }

        // --------------------- 结果输出（特定时间步） ---------------------
        string filename;
        if (tt == NT) {
            filename = "coord_disp_pd_1000.txt";
        } else if (tt == 675) {
            filename = "coord_disp_pd_675.txt";
        } else if (tt == 750) {
            filename = "coord_disp_pd_750.txt";
        } else if (tt == 825) {
            filename = "coord_disp_pd_825.txt";
        }

        if (!filename.empty()) {
            ofstream outFile(filename);
            if (outFile.is_open()) {
                outFile.precision(5);
                outFile << scientific;
                for (int i = 0; i < totint; ++i) {
                    outFile << coord[i].x << " " << coord[i].y << " "
                            << disp[i].x << " " << disp[i].y << " "
                            << dmg[i] << endl;
                }
                outFile.close();
                cout << "Output saved to " << filename << endl;
            } else {
                cerr << "Error opening file: " << filename << endl;
            }
        }
    }

    cout << "Simulation completed!" << endl;
    return 0;
}