#ifndef WEAPON_VIEW_RENDERER_H
#define WEAPON_VIEW_RENDERER_H

class Camera;
class Weapon;

class WeaponViewRenderer
{
public:
    void Render(Weapon* weapon, const Camera& camera, int screenWidth, int screenHeight);
};

#endif
