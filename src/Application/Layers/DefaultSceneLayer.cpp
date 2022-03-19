#include "DefaultSceneLayer.h"

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#include <GLM/gtc/random.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

#include <filesystem>

// Graphics
#include "Graphics/Buffers/IndexBuffer.h"
#include "Graphics/Buffers/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture2D.h"
#include "Graphics/Textures/TextureCube.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"
#include "Graphics/Framebuffer.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"
#include "Utils/GlmDefines.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"
#include "Gameplay/Components/Light.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/InputEngine.h"

#include "Application/Application.h"
#include "Gameplay/Components/ParticleSystem.h"
#include "Graphics/Textures/Texture3D.h"
#include "Graphics/Textures/Texture1D.h"

//Windows
#include "Application/Windows/DebugWindow.h"
#include "Application/Layers/ImGuiDebugLayer.h"
#include "Application/Layers/RenderLayer.h"

DefaultSceneLayer::DefaultSceneLayer() :
	ApplicationLayer()
{
	Name = "Default Scene";
	Overrides = AppLayerFunctions::OnAppLoad;
}

DefaultSceneLayer::~DefaultSceneLayer() = default;

void DefaultSceneLayer::OnAppLoad(const nlohmann::json& config) {
	_CreateScene();
}

void DefaultSceneLayer::_CreateScene()
{
	using namespace Gameplay;
	using namespace Gameplay::Physics;

	Application& app = Application::Get();

	bool loadScene = false;
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene && std::filesystem::exists("scene.json")) {
		app.LoadScene("scene.json");
	} else {
		 
		// Basic gbuffer generation with no vertex manipulation
		ShaderProgram::Sptr deferredForward = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});
		deferredForward->SetDebugName("Deferred - GBuffer Generation");  

		// Our foliage shader which manipulates the vertices of the mesh
		ShaderProgram::Sptr foliageShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/foliage.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});  
		foliageShader->SetDebugName("Foliage");   

		// This shader handles our multitexturing example
		ShaderProgram::Sptr multiTextureShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/vert_multitextured.glsl" },  
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_multitextured.glsl" }
		});
		multiTextureShader->SetDebugName("Multitexturing"); 

		// This shader handles our displacement mapping example
		ShaderProgram::Sptr displacementShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});
		displacementShader->SetDebugName("Displacement Mapping");

		// This shader handles our cel shading example
		ShaderProgram::Sptr celShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/cel_shader.glsl" }
		});
		celShader->SetDebugName("Cel Shader");


		// Load in the meshes
		MeshResource::Sptr monkeyMesh = ResourceManager::CreateAsset<MeshResource>("Monkey.obj");

		// Load in some textures
		Texture2D::Sptr    boxTexture   = ResourceManager::CreateAsset<Texture2D>("textures/box-diffuse.png");
		Texture2D::Sptr    boxSpec      = ResourceManager::CreateAsset<Texture2D>("textures/box-specular.png");
		Texture2D::Sptr    monkeyTex    = ResourceManager::CreateAsset<Texture2D>("textures/monkey-uvMap.png");
		Texture2D::Sptr    leafTex      = ResourceManager::CreateAsset<Texture2D>("textures/leaves.png");
		leafTex->SetMinFilter(MinFilter::Nearest);
		leafTex->SetMagFilter(MagFilter::Nearest);

#pragma region Basic Texture Creation
		Texture2DDescription singlePixelDescriptor;
		singlePixelDescriptor.Width = singlePixelDescriptor.Height = 1;
		singlePixelDescriptor.Format = InternalFormat::RGB8;

		float normalMapDefaultData[3] = { 0.5f, 0.5f, 1.0f };
		Texture2D::Sptr normalMapDefault = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		normalMapDefault->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, normalMapDefaultData);

		float solidBlack[3] = { 0.5f, 0.5f, 0.5f };
		Texture2D::Sptr solidBlackTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidBlackTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidBlack);

		float solidGrey[3] = { 0.0f, 0.0f, 0.0f };
		Texture2D::Sptr solidGreyTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidGreyTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidGrey);

		float solidWhite[3] = { 1.0f, 1.0f, 1.0f };
		Texture2D::Sptr solidWhiteTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidWhiteTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidWhite);

#pragma endregion 

		// Loading in a 1D LUT
		Texture1D::Sptr toonLut = ResourceManager::CreateAsset<Texture1D>("luts/toon-1D.png"); 
		toonLut->SetWrap(WrapMode::ClampToEdge);

		app.GetLayer<RenderLayer>()->SetDiffuseWarp(toonLut);

		app.GetLayer<RenderLayer>()->SetSpecularWarp(toonLut); 


		// Here we'll load in the cubemap, as well as a special shader to handle drawing the skybox
		TextureCube::Sptr testCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		ShaderProgram::Sptr      skyboxShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" } 
		});
		  
		// Create an empty scene
		Scene::Sptr scene = std::make_shared<Scene>();  

		// Setting up our enviroment map
		scene->SetSkyboxTexture(testCubemap); 
		scene->SetSkyboxShader(skyboxShader);
		// Since the skybox I used was for Y-up, we need to rotate it 90 deg around the X-axis to convert it to z-up 
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));


		

		// Loading in a color lookup table
		//HOW TO MAKE LUT
		Texture3D::Sptr coolLut = ResourceManager::CreateAsset<Texture3D>("luts/Icy.CUBE");
		Texture3D::Sptr warmLut = ResourceManager::CreateAsset<Texture3D>("luts/Toasty.CUBE");
		Texture3D::Sptr customLut = ResourceManager::CreateAsset<Texture3D>("luts/Sweet.CUBE");

		app.GetLayer<ImGuiDebugLayer>()->GetWindow<DebugWindow>()->SetCoolCC(coolLut);
		app.GetLayer<ImGuiDebugLayer>()->GetWindow<DebugWindow>()->SetWarmCC(warmLut);
		app.GetLayer<ImGuiDebugLayer>()->GetWindow<DebugWindow>()->SetCustomCC(customLut);

		// Configure the color correction LUT
		//HOW TO SET LUT
		scene->SetColorLUT(coolLut);


		

		// Create our materials
		// This will be our box material, with no environment reflections
		Material::Sptr boxMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			boxMaterial->Name = "Box";
			boxMaterial->Set("u_Material.AlbedoMap", boxTexture);
			boxMaterial->Set("u_Material.Shininess", 0.1f);
			boxMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr monkeyMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			monkeyMaterial->Name = "Monkey";
			monkeyMaterial->Set("u_Material.AlbedoMap", monkeyTex);
			monkeyMaterial->Set("u_Material.NormalMap", normalMapDefault);
			monkeyMaterial->Set("u_Material.Shininess", 0.5f);
		}

		// This will be the reflective material, we'll make the whole thing 50% reflective
		Material::Sptr testMaterial = ResourceManager::CreateAsset<Material>(deferredForward); 
		{
			testMaterial->Name = "Box-Specular";
			testMaterial->Set("u_Material.AlbedoMap", boxTexture); 
			testMaterial->Set("u_Material.Specular", boxSpec);
			testMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// Our foliage vertex shader material 
		Material::Sptr foliageMaterial = ResourceManager::CreateAsset<Material>(foliageShader);
		{
			foliageMaterial->Name = "Foliage Shader";
			foliageMaterial->Set("u_Material.AlbedoMap", leafTex);
			foliageMaterial->Set("u_Material.Shininess", 0.1f);
			foliageMaterial->Set("u_Material.DiscardThreshold", 0.1f);
			foliageMaterial->Set("u_Material.NormalMap", normalMapDefault);

			foliageMaterial->Set("u_WindDirection", glm::vec3(1.0f, 1.0f, 0.0f));
			foliageMaterial->Set("u_WindStrength", 0.5f);
			foliageMaterial->Set("u_VerticalScale", 1.0f);
			foliageMaterial->Set("u_WindSpeed", 1.0f);
		}

		//OEN DIMENSIONAL LUT
		// Our toon shader material
		Material::Sptr toonMaterial = ResourceManager::CreateAsset<Material>(celShader);
		{
			toonMaterial->Name = "Toon"; 
			toonMaterial->Set("u_Material.AlbedoMap", boxTexture);
			toonMaterial->Set("u_Material.NormalMap", normalMapDefault);
			toonMaterial->Set("s_ToonTerm", toonLut);
			toonMaterial->Set("u_Material.Shininess", 0.1f); 
			toonMaterial->Set("u_Material.Steps", 8);
		}
		

		Material::Sptr displacementTest = ResourceManager::CreateAsset<Material>(displacementShader);
		{
			Texture2D::Sptr displacementMap = ResourceManager::CreateAsset<Texture2D>("textures/displacement_map.png");
			Texture2D::Sptr normalMap       = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap      = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			displacementTest->Name = "Displacement Map";
			displacementTest->Set("u_Material.AlbedoMap", diffuseMap);
			displacementTest->Set("u_Material.NormalMap", normalMap);
			displacementTest->Set("s_Heightmap", displacementMap);
			displacementTest->Set("u_Material.Shininess", 0.5f);
			displacementTest->Set("u_Scale", 0.1f);
		}

		Material::Sptr normalmapMat = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			Texture2D::Sptr normalMap       = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap      = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			normalmapMat->Name = "Tangent Space Normal Map";
			normalmapMat->Set("u_Material.AlbedoMap", diffuseMap);
			normalmapMat->Set("u_Material.NormalMap", normalMap);
			normalmapMat->Set("u_Material.Shininess", 0.5f);
			normalmapMat->Set("u_Scale", 0.1f);
		}

		Material::Sptr multiTextureMat = ResourceManager::CreateAsset<Material>(multiTextureShader);
		{
			Texture2D::Sptr sand  = ResourceManager::CreateAsset<Texture2D>("textures/terrain/sand.png");
			Texture2D::Sptr grass = ResourceManager::CreateAsset<Texture2D>("textures/terrain/grass.png");

			multiTextureMat->Name = "Multitexturing";
			multiTextureMat->Set("u_Material.DiffuseA", sand);
			multiTextureMat->Set("u_Material.DiffuseB", grass);
			multiTextureMat->Set("u_Material.NormalMapA", normalMapDefault);
			multiTextureMat->Set("u_Material.NormalMapB", normalMapDefault);
			multiTextureMat->Set("u_Material.Shininess", 0.5f);
			multiTextureMat->Set("u_Scale", 0.1f); 
		}

		// Create some lights for our scene
		GameObject::Sptr lightParent = scene->CreateGameObject("Lights");

		for (int ix = 0; ix < 50; ix++) {
			GameObject::Sptr light = scene->CreateGameObject("Light");
			light->SetPostion(glm::vec3(glm::diskRand(25.0f), 1.0f));
			lightParent->AddChild(light);

			Light::Sptr lightComponent = light->Add<Light>();
			lightComponent->SetColor(glm::linearRand(glm::vec3(0.0f), glm::vec3(1.0f)));
			lightComponent->SetRadius(glm::linearRand(0.1f, 10.0f));
			lightComponent->SetIntensity(glm::linearRand(1.0f, 2.0f));
		}

		// We'll create a mesh that is a simple plane that we can resize later
		MeshResource::Sptr planeMesh = ResourceManager::CreateAsset<MeshResource>();
		planeMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(1.0f)));
		planeMesh->GenerateMesh();

		MeshResource::Sptr sphere = ResourceManager::CreateAsset<MeshResource>();
		sphere->AddParam(MeshBuilderParam::CreateIcoSphere(ZERO, ONE, 5));
		sphere->GenerateMesh();

		// Set up the scene's camera
		GameObject::Sptr camera = scene->MainCamera->GetGameObject()->SelfRef();
		{
			camera->SetPostion({ -1, -1, 0.5 });
			camera->LookAt(glm::vec3(0.0f, 0.0f, 0.5f));

			camera->Add<SimpleCameraControl>();

			
			// This is now handled by scene itself!
			//Camera::Sptr cam = camera->Add<Camera>();
			// Make sure that the camera is set as the scene's main camera!
			//scene->MainCamera = cam;
		}
		
		// Set up all our sample objects
		GameObject::Sptr plane = scene->CreateGameObject("Plane");
		{
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(boxMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		

		GameObject::Sptr barrel = scene->CreateGameObject("Barrel");
		{
			RotatingBehaviour::Sptr rotate = barrel->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, -9.f);
		}

		GameObject::Sptr outerBarrel = scene->CreateGameObject("Outer Barrel");
		{
			RotatingBehaviour::Sptr rotate = outerBarrel->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, 9.f);
		}

		GameObject::Sptr monkey1 = scene->CreateGameObject("Monkey 1");
		{
			// Set position in the scene
			monkey1->SetPostion(glm::vec3(5.0f, 0.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey1->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey1->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey1->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey1->Add<TriggerVolumeEnterBehaviour>();

			RotatingBehaviour::Sptr rotate = monkey1->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, 27.f);

			barrel->AddChild(monkey1);
		}

		GameObject::Sptr monkey2 = scene->CreateGameObject("Monkey 2");
		{
			// Set position in the scene
			monkey2->SetPostion(glm::vec3(-5.0f, 0.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey2->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey2->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey2->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey2->Add<TriggerVolumeEnterBehaviour>();

			RotatingBehaviour::Sptr rotate = monkey2->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, 27.f);

			barrel->AddChild(monkey2);
		}


		GameObject::Sptr monkey3 = scene->CreateGameObject("Monkey 3");
		{
			// Set position in the scene
			monkey3->SetPostion(glm::vec3(0.0f, 5.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey3->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey3->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey3->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey3->Add<TriggerVolumeEnterBehaviour>();

			RotatingBehaviour::Sptr rotate = monkey3->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, 27.f);

			barrel->AddChild(monkey3);
		}


		GameObject::Sptr monkey4 = scene->CreateGameObject("Monkey 4");
		{
			// Set position in the scene
			monkey4->SetPostion(glm::vec3(0.0f, -5.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey4->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey4->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey4->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey4->Add<TriggerVolumeEnterBehaviour>();

			RotatingBehaviour::Sptr rotate = monkey4->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, 27.f);

			barrel->AddChild(monkey4);
		}


		GameObject::Sptr monkey5 = scene->CreateGameObject("Monkey 5");
		{
			// Set position in the scene
			monkey5->SetPostion(glm::vec3(8.0f, 8.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey5->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey5->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey5->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey5->Add<TriggerVolumeEnterBehaviour>();

			RotatingBehaviour::Sptr rotate = monkey5->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, -27.f);

			outerBarrel->AddChild(monkey5);
		}

		GameObject::Sptr monkey6 = scene->CreateGameObject("Monkey 6");
		{
			// Set position in the scene
			monkey6->SetPostion(glm::vec3(-8.0f, 8.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey6->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey6->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey6->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey6->Add<TriggerVolumeEnterBehaviour>();

			RotatingBehaviour::Sptr rotate = monkey6->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, -27.f);

			outerBarrel->AddChild(monkey6);
		}


		GameObject::Sptr monkey7 = scene->CreateGameObject("Monkey 7");
		{
			// Set position in the scene
			monkey7->SetPostion(glm::vec3(8.0f, -8.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey7->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey7->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey7->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey7->Add<TriggerVolumeEnterBehaviour>();

			RotatingBehaviour::Sptr rotate = monkey7->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, -27.f);

			outerBarrel->AddChild(monkey7);
		}


		GameObject::Sptr monkey8 = scene->CreateGameObject("Monkey 8");
		{
			// Set position in the scene
			monkey8->SetPostion(glm::vec3(-8.0f, -8.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey8->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey8->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey8->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey8->Add<TriggerVolumeEnterBehaviour>();

			RotatingBehaviour::Sptr rotate = monkey8->Add<RotatingBehaviour>();
			rotate->RotationSpeed = glm::vec3(0.0f, 0.0f, -27.f);

			outerBarrel->AddChild(monkey8);
		}
		
		

		/////////////////////////// UI //////////////////////////////
		/*
		GameObject::Sptr canvas = scene->CreateGameObject("UI Canvas");
		{
			RectTransform::Sptr transform = canvas->Add<RectTransform>();
			transform->SetMin({ 16, 16 });
			transform->SetMax({ 256, 256 });

			GuiPanel::Sptr canPanel = canvas->Add<GuiPanel>();


			GameObject::Sptr subPanel = scene->CreateGameObject("Sub Item");
			{
				RectTransform::Sptr transform = subPanel->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 128, 128 });

				GuiPanel::Sptr panel = subPanel->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

				panel->SetTexture(ResourceManager::CreateAsset<Texture2D>("textures/upArrow.png"));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 16.0f);
				font->Bake();

				GuiText::Sptr text = subPanel->Add<GuiText>();
				text->SetText("Hello world!");
				text->SetFont(font);

				monkey1->Get<JumpBehaviour>()->Panel = text;
			}

			canvas->AddChild(subPanel);
		}
		*/

		

		GuiBatcher::SetDefaultTexture(ResourceManager::CreateAsset<Texture2D>("textures/ui-sprite.png"));
		GuiBatcher::SetDefaultBorderRadius(8);

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("scene-manifest.json");
		// Save the scene to a JSON file
		scene->Save("scene.json");

		// Send the scene to the application
		app.LoadScene(scene);
	}
}
