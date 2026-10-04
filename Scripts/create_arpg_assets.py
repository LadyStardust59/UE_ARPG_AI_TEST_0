"""Run once with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Creates only /Game/ARPG assets. Existing template assets are never overwritten.
"""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

for folder in ["/Game/ARPG/Maps", "/Game/ARPG/Data", "/Game/ARPG/Materials"]:
    editor_assets.make_directory(folder)

hero_class = unreal.load_class(None, "/Script/MyCppProject.ARPGHeroDefinition")
mode_class = unreal.load_class(None, "/Script/MyCppProject.ARPGGameMode")
assert hero_class and mode_class, "Build the editor target before running this script"

for name, label, heavy, hp, speed, damage, interval, color in [
    ("DA_Striker", "疾风", False, 220., 650., 24., .38, unreal.LinearColor(.1,.7,1.,1.)),
    ("DA_Breaker", "磐石", True, 300., 520., 36., .58, unreal.LinearColor(1.,.5,.1,1.)),
]:
    path = "/Game/ARPG/Data/" + name
    definition = editor_assets.load_asset(path)
    if not definition:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", hero_class)
        definition = asset_tools.create_asset(name, "/Game/ARPG/Data", hero_class, factory)
    for prop, value in dict(display_name=label, heavy_style=heavy, max_health=hp,
                            walk_speed=speed, attack_damage=damage, attack_interval=interval,
                            accent=color).items():
        definition.set_editor_property(prop, value)
    editor_assets.save_loaded_asset(definition)

cube = unreal.load_asset("/Engine/BasicShapes/Cube")

def box(name, location, scale):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor

for map_name in ["L_MainMenu", "L_Arena", "L_Fortress"]:
    package = "/Game/ARPG/Maps/" + map_name
    if editor_assets.does_asset_exist(package):
        unreal.log("ARPG_ASSET_EXISTS " + package)
        continue
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    world.get_world_settings().set_editor_property("default_game_mode", mode_class)
    box("ArenaFloor", (0,0,-60), (54,48,1))
    for label, pos, scale in [
        ("NorthWall",(0,2450,100),(56,1,3)),
        ("SouthWall",(0,-2450,100),(56,1,3)),
        ("EastWall",(2750,0,100),(1,50,3)),
        ("WestWall",(-2750,0,100),(1,50,3)),
    ]:
        box(label,pos,scale)
    for x in [-2500,2500]:
        for y in [-2100,2100]:
            box("BoundaryPillar",(x,y,180),(1.5,1.5,4.8))
    if map_name == "L_Fortress":
        for y in [-1800,1800]:
            box("FortressRampart",(2150,y,250),(3,5,6))
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,900),unreal.Rotator(-55,-30,0))
    sun.light_component.set_editor_property("intensity",3.)
    sky = actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,600))
    sky.light_component.set_editor_property("intensity",1.2)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector(0,0,0))
    actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-1200,0,110))
    saved = unreal.EditorLoadingAndSavingUtils.save_map(world,package)
    assert saved, "Could not save " + package
    unreal.log("ARPG_MAP_SAVED " + package)

unreal.log("ARPG_ASSETS_COMPLETE")
