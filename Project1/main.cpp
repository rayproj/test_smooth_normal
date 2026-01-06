#include <fbxsdk.h>
#include <string>
#include <map>
using namespace fbxsdk;
using namespace std;

bool IsAlmostEqual(const FbxVector4& v1, const FbxVector4& v2, double tolerance = 1e-6) {
	return fabs(v1[0] - v2[0]) < tolerance &&
		fabs(v1[1] - v2[1]) < tolerance &&
		fabs(v1[2] - v2[2]) < tolerance;
}

void StoreNormalsToVertColor(FbxNode* node)
{
	if (node->GetChildCount())
	{
		for (int i = 0; i < node->GetChildCount(); i++)
		{
			if (node->GetChild(i)->GetMesh())
			{
				//获取mesh
				FbxMesh* mesh = node->GetChild(i)->GetMesh();

				int num_vertices = mesh->GetPolygonVertexCount();

				// 如果不存在 自动生成切线 副切线
				if (mesh->GetElementTangentCount() == 0 || mesh->GetElementBinormalCount() == 0)
				{
					mesh->GenerateTangentsData(0, true);
				}

				//获取layer，顶点色、法切线之类的顶点信息几乎存在layer中
				fbxsdk::FbxLayer* layer0 = mesh->GetLayer(0);

				//依次获取layer中的顶点色层、法线层、切线层、副法线（或者叫副切线）层
				fbxsdk::FbxLayerElementVertexColor* VertColor = layer0->GetVertexColors();
				//VertColor->SetMappingMode(FbxGeometryElement::eByControlPoint);
				//VertColor->SetReferenceMode(FbxGeometryElement::eDirect);

				std::map<int, FbxVector4> fbxVectorMap;

				fbxsdk::FbxLayerElementNormal* VertNormal = layer0->GetNormals();
				fbxsdk::FbxLayerElementTangent* VertTangent = layer0->GetTangents();
				fbxsdk::FbxLayerElementBinormal* VertBinomral = layer0->GetBinormals();
				FbxVector4* controlPoints = mesh->GetControlPoints();
				//逐顶点遍历操作
				for (int j = 0; j < num_vertices; j++)
				{
					//声明一个整型数组，用于存放与当前遍历顶点同属一个控制点的顶点序列
					//数组用的是FbxSdk内置的数组，是动态数组，比较好使
					FbxArray<int> SameControlPointsIndex;
					for (int k = 0; k < num_vertices; k++)
					{
						auto kpvIndex = mesh->GetPolygonVertices()[k];
						auto jpvIndex = mesh->GetPolygonVertices()[j];
						if (kpvIndex == jpvIndex)
						{
							SameControlPointsIndex.Add(k);
						}
						else {
							// 获取顶点k的位置
							FbxVector4 kPosition = controlPoints[kpvIndex];
							// 获取当前顶点的位置
							FbxVector4 currentPosition = controlPoints[jpvIndex];
							// 比较位置是否相同（浮点数比较需考虑误差）
							if (IsAlmostEqual(kPosition, currentPosition)) {
								SameControlPointsIndex.Add(k);
							}
						}
					}

					//声明一个Vector4数组，获取并存放上面声明的顶点序列数组中所有不同方向的法线
					//需要注意的是，与Unity的顶点不同，这里的顶点中有很多法线的方向是重复的
					//如果将重复的法线也参与计算则算出来的值是错误的，轮廓线会扭曲，说出来都是泪
					//所以使用AddUnique保证去掉重复的法线方向
					FbxArray<FbxVector4> Normals;
					for (int x = 0; x < SameControlPointsIndex.Size(); x++)
					{
						FbxVector4 Normal = VertNormal->GetDirectArray()[SameControlPointsIndex[x]];
						Normals.AddUnique(Normal);
					}
					//将所有不同方向的法线加在一起并归一化获得光滑法线
					FbxVector4 SmoothNormal;
					for (int n = 0; n < Normals.Size(); n++)
					{
						SmoothNormal += Normals[n];
					}
					SmoothNormal.Normalize();
					fbxVectorMap[j] = SmoothNormal;

					int VertColorIndex = VertColor->GetIndexArray()[j];
					FbxColor color;
					color.mRed = SmoothNormal[0] * 0.5f + 0.5f;
					color.mGreen = SmoothNormal[1] * 0.5f + 0.5f;
					color.mBlue = SmoothNormal[2] * 0.5f + 0.5f;
					color.mAlpha = VertColor->GetDirectArray()[VertColorIndex].mAlpha;
					//将颜色写入顶点颜色layer中
					VertColor->GetDirectArray().SetAt(VertColorIndex, color);

					//分别获取当前顶点的切线、法线、副切线用于构建模型→切线空间的转换矩阵
					//需要注意的是：法线、切线、副切线的映射方式（也就是存储方式）是与顶点
					//序列一一对应，所以直接GetDirectArray()[顶点序号]就可以

					//FbxVector4 Tangent = VertTangent->GetDirectArray()[j];
					//FbxVector4 Normal = VertNormal->GetDirectArray()[j];
					//FbxVector4 Bitangent = VertBinomral->GetDirectArray()[j];

					////FbxVector4 Bitangent = Normal.CrossProduct(Tangent) * Tangent[3];
					////Bitangent.Normalize();


					////将法线从模型空间转为切线空间
					////FbxSdk的内置矩阵类型不会使，算出来的值有问题，所以还是手动计算
					//FbxVector4 tmpVector;
					//tmpVector = SmoothNormal;
					//tmpVector[0] = Tangent.DotProduct(SmoothNormal);
					//tmpVector[1] = Bitangent.DotProduct(SmoothNormal);
					//tmpVector[2] = Normal.DotProduct(SmoothNormal);
					//tmpVector[3] = 0;
					//SmoothNormal = tmpVector;
					//SmoothNormal.Normalize();

					////获取当前顶点的颜色信息存放于其layer中的序号
					////与法切副不同，顶点色数据在layer中的存储方式（映射Mapping方式）稍微复杂
					////首先要使用GetIndexArray()[顶点序号]获取其颜色值在DirectArray中的序号
					////然后使用GetDirectArray()[获得的序号]来获得该顶点的顶点色信息
					//int VertColorIndex = VertColor->GetIndexArray()[j];
					////声明一个颜色值，将法线数值范围从-1~1处理为0~1后存入RGB通道中，A通道保持
					////不变，因为其中存放着轮廓线大小信息
					//FbxColor color;
					//color.mRed = SmoothNormal[0] * 0.5f + 0.5f;
					//color.mGreen = SmoothNormal[1] * 0.5f + 0.5f;
					//color.mBlue = SmoothNormal[2] * 0.5f + 0.5f;
					//color.mAlpha = VertColor->GetDirectArray()[VertColorIndex].mAlpha;
					////将颜色写入顶点颜色layer中
					//VertColor->GetDirectArray().SetAt(VertColorIndex, color);
				}

				for (const auto& pair : fbxVectorMap) {
					//VertNormal->GetDirectArray().SetAt(pair.first, pair.second);
					//VertTangent->GetDirectArray().SetAt(pair.first, pair.second);
					/*int VertColorIndex = VertColor->GetIndexArray()[pair.first];
					FbxColor color;
					auto SmoothNormal = pair.second;
					color.mRed = SmoothNormal[0] * 0.5f + 0.5f;
					color.mGreen = SmoothNormal[1] * 0.5f + 0.5f;
					color.mBlue = SmoothNormal[2] * 0.5f + 0.5f;
					color.mAlpha = VertColor->GetDirectArray()[VertColorIndex].mAlpha;
					VertColor->GetDirectArray().SetAt(VertColorIndex, color);*/
				}
			}
			//递归调用，确保场景中所有mesh都得到处理
			StoreNormalsToVertColor(node->GetChild(i));
		}
	}
}

// 如果没有是顶点色 先手动添加
void AddVertColor(FbxNode* node)
{
	if (node->GetChildCount())
	{
		for (int i = 0; i < node->GetChildCount(); i++)
		{
			if (node->GetChild(i)->GetMesh())
			{
				//获取mesh
				FbxMesh* mesh = node->GetChild(i)->GetMesh();

				int num_vertices = mesh->GetPolygonVertexCount();

				auto LayerZero = mesh->GetLayer(0);

				fbxsdk::FbxLayerElementVertexColor* VertexColor = fbxsdk::FbxLayerElementVertexColor::Create(mesh, "");
				VertexColor->SetMappingMode(fbxsdk::FbxLayerElement::eByPolygonVertex);
				VertexColor->SetReferenceMode(fbxsdk::FbxLayerElement::eIndexToDirect);
				FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
				FbxLayerElementArrayTemplate<int>& VertexIndexArray = VertexColor->GetIndexArray();
				LayerZero->SetVertexColors(VertexColor);

				for (int VertIndex = 0; VertIndex < num_vertices; VertIndex++)
				{
					VertexColorArray.Add(FbxColor(1, 0, 0, 1));
					VertexIndexArray.Add(VertIndex);
				}


			}
			//递归调用，确保场景中所有mesh都得到处理
			AddVertColor(node->GetChild(i));
		}
	}
}

int main(int argc, char** argv) {

	for (int i = 0; i < argc; i++)
	{
		/*string name = argv[i];
		string name_export = name.replace(name.find(".FBX"), 4, "_outline.FBX");
		printf("%s \n", name_export.c_str());*/


		// lFilename是输入路径，lFilename2是输出路径
		const char* lFilename = "C:\\Users\\xxx\\Desktop\\dsadas.FBX";
		const char* lFilename2 = "C:\\Users\\xxxx\\Desktop\\export.FBX";

		//char* lFilename = argv[i];
		//char* lFilename2 = argv[i];


		//主函数中几乎都是FbxSdk文档中所写的代码，是导入导出fbx所需要的的标准流程

		// Initialize the SDK manager. This object handles all our memory management.
		FbxManager* lSdkManager = FbxManager::Create();

		// Create the IO settings object.
		FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
		ios->SetBoolProp(EXP_TANGENTSPACE, true);
		ios->SetBoolProp(IMP_FBX_TANGENT, true);
		ios->SetBoolProp(EXP_AUTOTANGENTSONLY, true);
		lSdkManager->SetIOSettings(ios);

		// Create an importer using the SDK manager.
		FbxImporter* lImporter = FbxImporter::Create(lSdkManager, "");

		// Use the first argument as the filename for the importer.
		if (!lImporter->Initialize(lFilename, -1, lSdkManager->GetIOSettings())) {
			printf("Call to FbxImporter::Initialize() failed.\n");
			printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString());
			exit(-1);
		}

		// Create a new scene so that it can be populated by the imported file.
		FbxScene* lScene = FbxScene::Create(lSdkManager, "myScene");

		// Import the contents of the file into the scene.
		lImporter->Import(lScene);

		// The file is imported; so get rid of the importer.
		lImporter->Destroy();

		//获取场景中根节点，然后对其调用自定义的StoreNormalsToVertColor函数	
		FbxNode* lRootNode = lScene->GetRootNode();
		if (lRootNode) {
			AddVertColor(lRootNode);
			StoreNormalsToVertColor(lRootNode);
		}

		//导出Fbx文件

		int lFormat = lSdkManager->GetIOPluginRegistry()->FindWriterIDByDescription("FBX ascii (*.fbx)");
		lFormat = -1;

		FbxExporter* lExporter = FbxExporter::Create(lSdkManager, "");
		bool lExportStatus = lExporter->Initialize(lFilename2, lFormat, lSdkManager->GetIOSettings());
		if (!lExportStatus) {
			printf("Call to FbxExporter::Initialize() failed.\n");
			printf("Error returned: %s\n\n", lExporter->GetStatus().GetErrorString());
			return false;
		}
		lExporter->Export(lScene);
		lExporter->Destroy();

		// Destroy the SDK manager and all the other objects it was handling.
		lSdkManager->Destroy();
	}

	return 0;
}

