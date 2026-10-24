#include "../Source/SurgeryHeaderLayout.h"
#include <iostream>
int main(){
    for(int w:{840,1120,1800}){
        const auto l=surgery::ui::headerLayout(w);
        const surgery::ui::Rect header{12,9,w-24,43};
        const auto inside=[&](surgery::ui::Rect r){
            return r.x>=header.x&&r.y>=header.y&&
                   r.right()<=header.right()&&r.bottom()<=header.bottom();
        };
        if(!inside(l.title)||!inside(l.brand)||!inside(l.theme)
            ||l.theme.intersects(l.title)||l.theme.intersects(l.brand)
            ||l.title.intersects(l.brand)
            ||l.theme.bottom()>=l.rootsY){
            std::cerr<<"THEME overlaps the header/root tabs at width "<<w<<"\n";
            return 1;
        }
    }
    std::cout<<"Surgery header overlap proofs passed\n";
    return 0;
}
