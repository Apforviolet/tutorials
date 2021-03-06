#include <vector>
#include <iostream>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"
#include "OpenNL_psm.h"

const int width  = 1920;
const int height = 1280;
const TGAColor white  (255, 255, 255, 255);
const TGAColor green  (  0, 255,   0, 255);
const TGAColor red    (255,   0,   0, 255);
const TGAColor blue   (  0,   0, 255, 255);
const TGAColor yellow (255, 255,   0, 255);
const TGAColor magenta(255,   0, 255, 255);
const TGAColor teal   ( 23, 167, 137, 255);
const TGAColor brown  (160,  64,   0, 255);

const TGAColor colors[] = {green, blue, yellow, magenta, teal, brown};
const int ncolors = sizeof(colors)/sizeof(TGAColor);


bool is_edge_present(Vec3f v1, Vec3f v2, Model &m) {
    for (int j=0; j<m.nhalfedges(); j++) {
        Vec3f u1 = m.point(m.from(j));
        Vec3f u2 = m.point(m.to  (j));
        float threshold = 1e-3;
        if (((u1-v1).norm()<threshold && (u2-v2).norm()<threshold) || ((u2-v1).norm()<threshold && (u1-v2).norm()<threshold)) {
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if (argc<2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }


#if 0
    {
        std::vector<Model> m;
        for (int i=1; i<argc; i++) {
            m.push_back(Model(argv[i]));
        }
        Vec3f min, max;
        m[0].get_bbox(min, max);
        float maxside = std::max(max.x-min.x, std::max(max.y-min.y, max.z-min.z));
        for (int i=0; i<(int)m.size(); i++) {
            std::cout << "= " << argv[1+i] << std::endl;
            for (int v=0; v<m[i].nverts(); v++) {
                Vec3f p = m[i].point(v);
                p  = (p - min)/maxside;
                m[i].point(v) = Vec3f(p.x, p.z, p.y);
            }
            std::cout << m[i];
        }
    }
    return 0;
#endif


    std::vector<Model> m;
    for (int i=1; i<argc; i++) {
        m.push_back(Model(argv[i]));
    }


    // fill faults/horizons halfedge attributes

    std::vector<bool> faults(m[0].nhalfedges(), false);
    std::vector<int> horizon(m[0].nhalfedges(), -1);
    const int nhorizons = m.size()-2;

    for (int i=0; i<m[0].nhalfedges(); i++) {
        Vec3f v[2] = { m[0].point(m[0].from(i)), m[0].point(m[0].to(i)) };

        faults[i] = m.size()>=2 && is_edge_present(v[0], v[1], m[1]);

        for (int j=0; j<nhorizons; j++) {
            if (is_edge_present(v[0], v[1], m[j+2])) {
                horizon[i] = j;
                break;
            }
        }
    }

    std::vector<int> fault_partner(m[0].nhalfedges(), -1);
    for (int i=0; i<m[0].nhalfedges(); i++) {
        Vec3f v1 = m[0].point(m[0].from(i));
        Vec3f v2 = m[0].point(m[0].to  (i));
        if (!faults[i]) continue;
        for (int j=0; j<m[0].nhalfedges(); j++) {
            if (!faults[j]) continue;
            Vec3f u1 = m[0].point(m[0].from(j));
            Vec3f u2 = m[0].point(m[0].to  (j));
            float threshold = 1e-3;
            if ((u1-v2).norm()<threshold && (u2-v1).norm()<threshold) {
                fault_partner[i] = j;
                fault_partner[j] = i;
                break;
            }
        }
    }

    if (1) {
        nlNewContext();
        nlSolverParameteri(NL_NB_VARIABLES, m[0].nverts());
        nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
        nlBegin(NL_SYSTEM);
        nlBegin(NL_MATRIX);

        for (int i=0; i<m[0].nhalfedges(); i++) {
            int v1 = m[0].from(i);
            int v2 = m[0].to(i);

            if (faults[i] && fault_partner[i]>=0 && i<fault_partner[i]) {
                int u1 = m[0].from(fault_partner[i]);
                int u2 = m[0].to(fault_partner[i]);
                float scale = 30;
                nlBegin(NL_ROW);
                nlCoefficient(v1,  scale);
                nlCoefficient(u2, -scale);
                nlEnd(NL_ROW);
                nlBegin(NL_ROW);
                nlCoefficient(v2,  scale);
                nlCoefficient(u1, -scale);
                nlEnd(NL_ROW);
                continue;
            }
            float dot = (m[0].point(v2)-m[0].point(v1)).normalize()*Vec3f(0,1,0);
            if (m[0].opp(i)<0 && fabs(dot)>0.99) {
                float scale = 30;
                nlBegin(NL_ROW);
                nlCoefficient(v1,  scale);
                nlRightHandSide(dot>0?scale:0);
                nlEnd(NL_ROW);
                nlBegin(NL_ROW);
                nlCoefficient(v2,  scale);
                nlRightHandSide(dot>0?scale:0);
                nlEnd(NL_ROW);
                continue;
            }
            nlBegin(NL_ROW);
            if (faults[i]) {
                nlCoefficient(v1,  100);
                nlCoefficient(v2, -100);
            } else {
                nlCoefficient(v1,  1);
                nlCoefficient(v2, -1);
                nlRightHandSide(m[0].point(v1).x-m[0].point(v2).x);
            }
            nlEnd(NL_ROW);
        }

        nlEnd(NL_MATRIX);
        nlEnd(NL_SYSTEM);
        nlSolve();
        for (int i=0; i<m[0].nverts(); i++) {
            m[0].point(i).x = nlGetVariable(i);
        }
    }

    if (1) {
        nlNewContext();
        nlSolverParameteri(NL_NB_VARIABLES, m[0].nverts()+nhorizons);
        nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
        nlBegin(NL_SYSTEM);
        nlBegin(NL_MATRIX);

        for (int i=0; i<m[0].nhalfedges(); i++) {
            int v1 = m[0].from(i);
            int v2 = m[0].to(i);

            if (0&& faults[i] && fault_partner[i]>=0 && i<fault_partner[i]) {
                int u1 = m[0].from(fault_partner[i]);
                int u2 = m[0].to(fault_partner[i]);
                float scale = 0.3;
                nlBegin(NL_ROW);
                nlCoefficient(v1,  scale);
                nlCoefficient(u2, -scale);
                nlEnd(NL_ROW);
                nlBegin(NL_ROW);
                nlCoefficient(v2,  scale);
                nlCoefficient(u1, -scale);
                nlEnd(NL_ROW);
                continue;
            }

            float dot = (m[0].point(v2)-m[0].point(v1)).normalize()*Vec3f(1,0,0);
            if (m[0].opp(i)<0 && fabs(dot)>0.99) {
                float scale = .3;
                nlBegin(NL_ROW);
                nlCoefficient(v1,  scale);
                nlRightHandSide(dot<0?scale:0);
                nlEnd(NL_ROW);
                nlBegin(NL_ROW);
                nlCoefficient(v2,  scale);
                nlRightHandSide(dot<0?scale:0);
                nlEnd(NL_ROW);
                continue;
            }
            if (horizon[i]>=0) {
                float scale = 30;
                nlBegin(NL_ROW);
                nlCoefficient(v1,  scale);
                nlCoefficient(m[0].nverts()+horizon[i], -scale);
                nlEnd(NL_ROW);
                nlBegin(NL_ROW);
                nlCoefficient(v2,  scale);
                nlCoefficient(m[0].nverts()+horizon[i], -scale);
                nlEnd(NL_ROW);
                continue;
            } 
            nlBegin(NL_ROW);
            nlCoefficient(v1,  1);
            nlCoefficient(v2, -1);
            nlRightHandSide(m[0].point(v1).y-m[0].point(v2).y);
            nlEnd(NL_ROW);
        }

        nlEnd(NL_MATRIX);
        nlEnd(NL_SYSTEM);
        nlSolve();
        for (int i=0; i<m[0].nverts(); i++) {
            m[0].point(i).y = nlGetVariable(i);
        }
    }


    // draw the mesh

    TGAImage frame(width, height, TGAImage::RGB);
    for (int i=0; i<m[0].nhalfedges(); i++) {
        Vec3f v[2] = { m[0].point(m[0].from(i)), m[0].point(m[0].to(i)) };
        TGAColor color = white;

        if (faults[i]) color = red;
        if (horizon[i]>=0) color = colors[horizon[i]%ncolors];

        Vec2i s[2];
        for (int j=0; j<2; j++) {
            s[j] = Vec2i(v[j].x*width, v[j].y*height);
        }

        frame.line(s[0], s[1], color);
    }

    frame.write_tga_file("framebuffer.tga");
//    std::cout << m[0];
    return 0;
}

