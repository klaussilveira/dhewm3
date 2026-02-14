#ifndef IMGUI_DISABLE

#include "sys/platform.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include "framework/Common.h"
#include "framework/Game.h"
#include "framework/DeclManager.h"
#include "LightEditor.h"

#include "sys/sys_imgui.h"
#include "../libs/imgui/imgui_internal.h"
#include "../libs/imgui/ImGuizmo.h"

#include "renderer/tr_local.h"

hcLightEditor* lightEditor = nullptr;

hcLightEditor::hcLightEditor( void ) {
	initialized = false;
	visible = false;
	selectedLight = nullptr;
	gizmoOperation = ImGuizmo::TRANSLATE;
	gizmoMode = ImGuizmo::WORLD;
	lightOrigin.Zero();
	lightAxis.Identity();
	lightColor.Set( 1.0f, 1.0f, 1.0f );
	lightRadius.Set( 300, 300, 300 );
	isPointLight = true;
	isParallel = false;
	noShadows = false;
	noSpecular = false;
	noDiffuse = false;
	falloffMode = 2;
	falloff = 1.0f;
	hasCenter = false;
	lightCenter.Zero();
	lightTarget.Zero();
	lightRight.Zero();
	lightUp.Zero();
	lightStart.Zero();
	lightEnd.Zero();
	explicitStartEnd = false;
	currentTextureIndex = 0;
	showTextureBrowser = false;
	textureFilter[0] = '\0';
	lastTextureFilter[0] = '\0';
	textureBrowserTab = 0;
	textureThumbnailSize = 64.0f;
	textureBrowserScrollToIdx = -1;
	propertyTableWidth = 150.0f;
}

hcLightEditor::~hcLightEditor( void ) {
	Shutdown();
}

void hcLightEditor::Init( const idDict* spawnArgs ) {
	initialized = false;
	selectedLight = nullptr;
	visible = true;

	common->ActivateTool( true );

	if ( gameEdit && gameEdit->PlayerIsValid() ) {
		idEntity* lastSelected = nullptr;
		int numSelected = gameEdit->GetSelectedEntities( &lastSelected, 1 );
		if ( numSelected > 0 ) {
			selectedLight = lastSelected;
			RefreshLightData();
		}
	}
}

void hcLightEditor::Shutdown( void ) {
	visible = false;
	selectedLight = nullptr;
	common->ActivateTool( false );
}

bool hcLightEditor::IsVisible( void ) const {
	return visible;
}

void hcLightEditor::SetVisible( bool vis ) {
	if ( vis && !visible ) {
		Init( nullptr );
	} else if ( !vis && visible ) {
		Shutdown();
	}

	visible = vis;
}

void hcLightEditor::LoadLightTextures( void ) {
	if ( textureNames.Num() > 0 ) {
		return;
	}

	int count = declManager->GetNumDecls( DECL_MATERIAL );
	for ( int i = 0; i < count; i++ ) {
		const idMaterial* mat = declManager->MaterialByIndex( i, false );
		idStr matName = mat->GetName();
		matName.ToLower();

		// Auto filter light and fog textures
		if ( matName.Icmpn( "lights/", 7 ) == 0 || matName.Icmpn( "fogs/", 5 ) == 0 ) {
			textureNames.Append( mat->GetName() );
		}
	}

	// Sort alphabetically
	for ( int i = 0; i < textureNames.Num() - 1; i++ ) {
		for ( int j = i + 1; j < textureNames.Num(); j++ ) {
			if ( textureNames[i].Icmp( textureNames[j] ) > 0 ) {
				idStr temp = textureNames[i];
				textureNames[i] = textureNames[j];
				textureNames[j] = temp;
			}
		}
	}
}

int hcLightEditor::FindTextureIndex( const char* textureName ) {
	if ( !textureName || !textureName[0] ) {
		return 0;
	}

	for ( int i = 0; i < textureNames.Num(); i++ ) {
		if ( textureNames[i].Icmp( textureName ) == 0 ) {
			return i + 1; // 0 is "None"
		}
	}
	return 0;
}

void hcLightEditor::RebuildFilteredTextureList( void ) {
	bool filterChanged = (idStr::Cmp( lastTextureFilter, textureFilter ) != 0) || (filteredTextureIndices.Num() == 0 && textureNames.Num() > 0);
	if ( filterChanged ) {
		filteredTextureIndices.Clear();
		idStr filterLower = textureFilter;
		filterLower.ToLower();

		for ( int i = 0; i < textureNames.Num(); i++ ) {
			if ( textureFilter[0] != '\0' ) {
				idStr nameLower = textureNames[i];
				nameLower.ToLower();
				if ( nameLower.Find( filterLower.c_str() ) == -1 ) {
					continue;
				}
			}

			filteredTextureIndices.Append( i );
		}

		idStr::Copynz( lastTextureFilter, textureFilter, sizeof(lastTextureFilter) );
	}
}

void hcLightEditor::OpenTextureBrowser( void ) {
	showTextureBrowser = true;
	textureFilter[0] = '\0';

	LoadLightTextures();

	// Reset filter cache
	filteredTextureIndices.Clear();
	lastTextureFilter[0] = '\0';

	// Scroll to current selection
	textureBrowserScrollToIdx = -1;
	if ( currentTextureIndex > 0 && currentTextureIndex <= textureNames.Num() ) {
		textureBrowserScrollToIdx = currentTextureIndex - 1;
	}
}

void hcLightEditor::RefreshLightData( void ) {
	if ( selectedLight == nullptr || gameEdit == nullptr ) {
		return;
	}

	// Get current transform
	gameEdit->EntityGetOrigin( selectedLight, lightOrigin );
	gameEdit->EntityGetAxis( selectedLight, lightAxis );

	// Reset to defaults first
	lightRadius.Set( 300, 300, 300 );
	lightTarget.Zero();
	lightRight.Zero();
	lightUp.Zero();
	lightStart.Zero();
	lightEnd.Zero();
	lightCenter.Zero();
	hasCenter = false;
	isParallel = false;
	explicitStartEnd = false;
	falloff = 1.0f;
	falloffMode = 2;

	// Get spawn args for other properties
	const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( selectedLight );
	if ( spawnArgs ) {
		// Rendering options
		noShadows = spawnArgs->GetBool( "noshadows", "0" );
		noSpecular = spawnArgs->GetBool( "nospecular", "0" );
		noDiffuse = spawnArgs->GetBool( "nodiffuse", "0" );

		// Falloff
		falloff = spawnArgs->GetFloat( "falloff", "1.0" );
		if ( falloff < 0.35f ) {
			falloffMode = 0;
		} else if ( falloff < 0.70f ) {
			falloffMode = 1;
		} else {
			falloffMode = 2;
		}

		// Texture
		lightTexture = spawnArgs->GetString( "texture", "" );
		currentTextureIndex = FindTextureIndex( lightTexture.c_str() );

		// Parallel light (sun)
		isParallel = spawnArgs->GetBool( "parallel", "0" );

		// Color
		if ( !spawnArgs->GetVector( "_color", "", lightColor ) ) {
			lightColor.Set( 1.0f, 1.0f, 1.0f );
		}

		// Always read light_radius so its not lost when switching types
		if ( !spawnArgs->GetVector( "light_radius", "", lightRadius ) ) {
			float radius = spawnArgs->GetFloat( "light" );
			if ( radius == 0 ) {
				radius = 300;
			}
			lightRadius.Set( radius, radius, radius );
		}

		// Always read light_center so its not lost when switching types
		if ( spawnArgs->GetVector( "light_center", "", lightCenter ) ) {
			hasCenter = true;
		}

		// Check if point light or projected light
		if ( spawnArgs->GetVector( "light_right", "", lightRight ) ) {
			// Projected light
			isPointLight = false;
			spawnArgs->GetVector( "light_target", "", lightTarget );
			spawnArgs->GetVector( "light_up", "", lightUp );

			if ( spawnArgs->GetVector( "light_start", "", lightStart ) ) {
				explicitStartEnd = true;
				if ( !spawnArgs->GetVector( "light_end", "", lightEnd ) ) {
					lightEnd = lightTarget;
				}
			} else {
				explicitStartEnd = false;
				lightStart = lightTarget * 0.25f;
				lightEnd = lightTarget;
			}
		} else {
			isPointLight = true;
		}
	}
}

void hcLightEditor::ApplyLightChanges( void ) {
	if ( selectedLight == nullptr || gameEdit == nullptr ) {
		return;
	}

	gameEdit->EntitySetOrigin( selectedLight, lightOrigin );
	gameEdit->EntitySetAxis( selectedLight, lightAxis );

	idDict newArgs;

	// Clear keys
	newArgs.Set( "falloff", "" );
	newArgs.Set( "texture", "" );
	newArgs.Set( "fog", "" );
	newArgs.Set( "light", "" );

	if ( isPointLight ) {
		newArgs.Set( "light_target", "" );
		newArgs.Set( "light_right", "" );
		newArgs.Set( "light_up", "" );
		newArgs.Set( "light_start", "" );
		newArgs.Set( "light_end", "" );
	}

	newArgs.SetVector( "origin", lightOrigin );
	newArgs.SetVector( "_color", lightColor );
	newArgs.Set( "noshadows", noShadows ? "1" : "0" );
	newArgs.Set( "nospecular", noSpecular ? "1" : "0" );
	newArgs.Set( "nodiffuse", noDiffuse ? "1" : "0" );
	newArgs.Set( "parallel", isParallel ? "1" : "0" );
	newArgs.SetFloat( "falloff", falloff );

	if ( lightTexture.Length() > 0 ) {
		newArgs.Set( "texture", lightTexture.c_str() );
	}

	if ( isPointLight ) {
		newArgs.Set( "light_radius", va( "%g %g %g", lightRadius.x, lightRadius.y, lightRadius.z ) );

		if ( hasCenter ) {
			newArgs.Set( "light_center", va( "%g %g %g", lightCenter.x, lightCenter.y, lightCenter.z ) );
		}
	} else {
		newArgs.Set( "light_target", va( "%g %g %g", lightTarget.x, lightTarget.y, lightTarget.z ) );
		newArgs.Set( "light_up", va( "%g %g %g", lightUp.x, lightUp.y, lightUp.z ) );
		newArgs.Set( "light_right", va( "%g %g %g", lightRight.x, lightRight.y, lightRight.z ) );

		if ( explicitStartEnd ) {
			newArgs.Set( "light_start", va( "%g %g %g", lightStart.x, lightStart.y, lightStart.z ) );
			newArgs.Set( "light_end", va( "%g %g %g", lightEnd.x, lightEnd.y, lightEnd.z ) );
		}
	}

	gameEdit->EntityChangeSpawnArgs( selectedLight, &newArgs );
	gameEdit->EntityUpdateChangeableSpawnArgs( selectedLight, nullptr );
	gameEdit->EntityUpdateVisuals( selectedLight );
}

void hcLightEditor::CheckSelectedLight( void ) {
	if ( gameEdit && gameEdit->PlayerIsValid() ) {
		idEntity* lastSelected = nullptr;
		int numSelected = gameEdit->GetSelectedEntities( &lastSelected, 1 );
		if ( numSelected > 0 && lastSelected != selectedLight ) {
			selectedLight = lastSelected;
			RefreshLightData();
		}
	}
}

void hcLightEditor::DrawGizmo( void ) {
	if ( selectedLight == nullptr || gameEdit == nullptr || !gameEdit->PlayerIsValid() ) {
		return;
	}

	if ( tr.primaryView == nullptr ) {
		return;
	}

	float* cameraView = tr.primaryView->worldSpace.modelViewMatrix;
	float* cameraProjection = tr.primaryView->projectionMatrix;

	idAngles lightAngles = lightAxis.ToAngles();
	idMat3 rotateMatrix = lightAngles.ToMat3();

	idMat4 gizmoMatrix( rotateMatrix, lightOrigin );
	idMat4 manipMatrix = gizmoMatrix.Transpose();

	// Set up ImGuizmo
	ImGuizmo::SetOrthographic( false );
	ImGuizmo::SetDrawlist( ImGui::GetBackgroundDrawList() );
	ImGuizmo::SetRect( 0, 0, (float)glConfig.vidWidth, (float)glConfig.vidHeight );

	// Draw and manipulate the gizmo
	if ( ImGuizmo::Manipulate( cameraView, cameraProjection,
			(ImGuizmo::OPERATION)gizmoOperation, (ImGuizmo::MODE)gizmoMode,
			manipMatrix.ToFloatPtr(), nullptr, nullptr ) ) {
		idMat4 resultMatrix = manipMatrix.Transpose();

		// Extract translation
		lightOrigin.x = resultMatrix[0][3];
		lightOrigin.y = resultMatrix[1][3];
		lightOrigin.z = resultMatrix[2][3];

		// Extract rotation
		idMat3 newRotation;
		newRotation[0][0] = resultMatrix[0][0];
		newRotation[0][1] = resultMatrix[1][0];
		newRotation[0][2] = resultMatrix[2][0];
		newRotation[1][0] = resultMatrix[0][1];
		newRotation[1][1] = resultMatrix[1][1];
		newRotation[1][2] = resultMatrix[2][1];
		newRotation[2][0] = resultMatrix[0][2];
		newRotation[2][1] = resultMatrix[1][2];
		newRotation[2][2] = resultMatrix[2][2];

		lightAngles = newRotation.ToAngles();
		lightAxis = lightAngles.ToMat3();

		ApplyLightChanges();
	}
}

void hcLightEditor::DrawGizmoControls( void ) {
	ImGui::Text( "Gizmo:" );
	ImGui::SameLine();

	if ( ImGui::RadioButton( "Translate", gizmoOperation == ImGuizmo::TRANSLATE ) ) {
		gizmoOperation = ImGuizmo::TRANSLATE;
	}

	ImGui::SameLine();
	if ( ImGui::RadioButton( "Rotate", gizmoOperation == ImGuizmo::ROTATE ) ) {
		gizmoOperation = ImGuizmo::ROTATE;
	}

	ImGui::SameLine();

	if ( ImGui::RadioButton( "Local", gizmoMode == ImGuizmo::LOCAL ) ) {
		gizmoMode = ImGuizmo::LOCAL;
	}

	ImGui::SameLine();

	if ( ImGui::RadioButton( "World", gizmoMode == ImGuizmo::WORLD ) ) {
		gizmoMode = ImGuizmo::WORLD;
	}
}

void hcLightEditor::DrawLightTypeSection( void ) {
	if ( ImGui::CollapsingHeader( "Light Type", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		bool changed = false;

		ImGui::PropertyTable( "LightTypeProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Type", [&]() {
				if ( ImGui::RadioButton( "Point", isPointLight ) ) {
					if ( !isPointLight ) {
						isPointLight = true;
						isParallel = false;
						changed = true;
					}
				}
				ImGui::AddTooltip( "Omnidirectional point light" );

				ImGui::SameLine();
				if ( ImGui::RadioButton( "Projected", !isPointLight ) ) {
					if ( isPointLight ) {
						isPointLight = false;
						isParallel = false;

						if ( lightTarget.LengthSqr() < 1.0f && lightRight.LengthSqr() < 1.0f ) {
							lightTarget.Set( 0, 0, -256 );
							lightUp.Set( 0, -128, 0 );
							lightRight.Set( -128, 0, 0 );
						}
						explicitStartEnd = false;
						changed = true;
					}
				}
				ImGui::AddTooltip( "Directional spotlight" );
			});

			if ( isPointLight ) {
				ImGui::LabeledWidget( "Parallel (Sun)", [&]() {
					if ( ImGui::Checkbox( "##parallel", &isParallel ) ) {
						if ( isParallel ) {
							hasCenter = true;
							if ( lightCenter.LengthSqr() < 0.01f ) {
								lightCenter.Set( 0, 0, 32 );
							}
						} else {
							hasCenter = false;
						}
						changed = true;
					}
					ImGui::AddTooltip( "Make this a parallel sun light" );
				});
			}
		});

		if ( changed ) {
			ApplyLightChanges();
			RefreshLightData();
		}
	}
}

void hcLightEditor::DrawTransformSection( void ) {
	if ( ImGui::CollapsingHeader( "Transform", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		bool changed = false;

		ImGui::PropertyTable( "TransformProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Position", [&]() {
				if ( ImGui::Vec3Widget( "##position", lightOrigin, "Light position in world coordinates" ) ) {
					changed = true;
				}
			});

			ImGui::LabeledWidget( "Rotation", [&]() {
				idAngles angles = lightAxis.ToAngles();
				idVec3 rotVec( angles.pitch, angles.yaw, angles.roll );
				if ( ImGui::Vec3Widget( "##rotation", rotVec, "Light rotation (pitch, yaw, roll)" ) ) {
					angles.pitch = rotVec.x;
					angles.yaw = rotVec.y;
					angles.roll = rotVec.z;
					lightAxis = angles.ToMat3();
					changed = true;
				}
			});
		});

		if ( changed ) {
			ApplyLightChanges();
		}
	}
}

void hcLightEditor::DrawColorSection( void ) {
	if ( ImGui::CollapsingHeader( "Color & Texture", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		bool changed = false;

		float maxVal = Max( Max( lightColor.x, lightColor.y ), lightColor.z );
		if ( maxVal < 0.001f ) {
			maxVal = 1.0f;
		}

		ImGui::PropertyTable( "ColorProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Color", [&]() {
				float displayCol[3] = {
					lightColor.x / maxVal,
					lightColor.y / maxVal,
					lightColor.z / maxVal
				};

				if ( ImGui::ColorEdit3( "##color", displayCol, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR ) ) {
					lightColor.x = displayCol[0] * maxVal;
					lightColor.y = displayCol[1] * maxVal;
					lightColor.z = displayCol[2] * maxVal;
					changed = true;
				}
				ImGui::AddTooltip( "Light color (RGB)" );
			});

			ImGui::LabeledWidget( "Brightness", [&]() {
				float brightness = maxVal;
				if ( ImGui::SliderFloatWidget( "##brightness", &brightness, 0.01f, 10.0f, "Overall light brightness multiplier" ) ) {
					if ( brightness > 0.001f ) {
						float scale = brightness / maxVal;
						lightColor.x *= scale;
						lightColor.y *= scale;
						lightColor.z *= scale;
					}
					changed = true;
				}
			});

			ImGui::LabeledWidget( "RGB Values", [&]() {
				if ( ImGui::Vec3Widget( "##rgbValues", lightColor, "Raw RGB values (can be > 1 for bright lights)" ) ) {
					changed = true;
				}
			});

			ImGui::LabeledWidget( "Texture", [&]() {
				LoadLightTextures();

				ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - 70 );
				if ( ImGui::BeginCombo( "##texture", currentTextureIndex == 0 ? "<None>" : lightTexture.c_str() ) ) {
					if ( ImGui::Selectable( "<None>", currentTextureIndex == 0 ) ) {
						currentTextureIndex = 0;
						lightTexture = "";
						changed = true;
					}

					for ( int i = 0; i < textureNames.Num(); i++ ) {
						bool isSelected = (currentTextureIndex == i + 1);
						if ( ImGui::Selectable( textureNames[i].c_str(), isSelected ) ) {
							currentTextureIndex = i + 1;
							lightTexture = textureNames[i];
							changed = true;
						}
						if ( isSelected ) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
					ImGui::AddTooltip( "Light projection texture" );
				}

				ImGui::UnlabeledWidget( [&]() {
					if ( ImGui::Button( "Browse..." ) ) {
						OpenTextureBrowser();
					}
				});
			});
		});

		if ( changed ) {
			ApplyLightChanges();
		}
	}
}

void hcLightEditor::DrawPointLightSection( void ) {
	if ( !isPointLight ) {
		return;
	}

	if ( ImGui::CollapsingHeader( "Point Light Settings", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		bool changed = false;
		bool equalRadius = (lightRadius.x == lightRadius.y && lightRadius.y == lightRadius.z);

		ImGui::PropertyTable( "PointLightProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Radius", [&]() {
				float uniformRadius = lightRadius.x;
				if ( ImGui::SliderFloatWidget( "##radius", &uniformRadius, 1.0f, 4096.0f, "Light radius (uniform)" ) ) {
					lightRadius.x = uniformRadius;
					lightRadius.y = uniformRadius;
					lightRadius.z = uniformRadius;
					changed = true;
				}
			});

			if ( !equalRadius ) {
				ImGui::LabeledWidget( "Radius XYZ", [&]() {
					if ( ImGui::Vec3Widget( "##radiusXYZ", lightRadius, "Light radius per axis" ) ) {
						changed = true;
					}
				});
			} else {
				ImGui::LabeledWidget( "", [&]() {
					if ( ImGui::TreeNode( "Per-Axis Radius" ) ) {
						if ( ImGui::Vec3Widget( "##radiusXYZ", lightRadius, "Light radius per axis" ) ) {
							changed = true;
						}
						ImGui::TreePop();
					}
				});
			}

			ImGui::LabeledWidget( "Has Center", [&]() {
				if ( ImGui::Checkbox( "##hasCenter", &hasCenter ) ) {
					if ( hasCenter && lightCenter.LengthSqr() < 0.01f ) {
						lightCenter.Set( 0, 0, 32 );
					}
					changed = true;
				}
				ImGui::AddTooltip( "Enable offset center point for the light" );
			});

			if ( hasCenter ) {
				ImGui::LabeledWidget( "Center", [&]() {
					if ( ImGui::Vec3Widget( "##center", lightCenter, "Light center offset from origin" ) ) {
						changed = true;
					}
				});
			}

			ImGui::LabeledWidget( "Falloff", [&]() {
				if ( ImGui::RadioButton( "None", falloffMode == 0 ) ) {
					falloffMode = 0;
					falloff = 0.0f;
					changed = true;
				}
				ImGui::SameLine();
				if ( ImGui::RadioButton( "Half", falloffMode == 1 ) ) {
					falloffMode = 1;
					falloff = 0.5f;
					changed = true;
				}
				ImGui::SameLine();
				if ( ImGui::RadioButton( "Full", falloffMode == 2 ) ) {
					falloffMode = 2;
					falloff = 1.0f;
					changed = true;
				}
				ImGui::AddTooltip( "Light intensity falloff curve" );
			});
		});

		if ( changed ) {
			ApplyLightChanges();
		}
	}
}

void hcLightEditor::DrawProjectedLightSection( void ) {
	if ( isPointLight ) {
		return;
	}

	if ( ImGui::CollapsingHeader( "Projected Light Settings", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		bool changed = false;

		ImGui::PropertyTable( "ProjectedLightProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Target", [&]() {
				if ( ImGui::Vec3Widget( "##target", lightTarget, "Light target direction vector" ) ) {
					changed = true;
				}
			});

			ImGui::LabeledWidget( "Right", [&]() {
				if ( ImGui::Vec3Widget( "##right", lightRight, "Light right vector (controls horizontal spread)" ) ) {
					changed = true;
				}
			});

			ImGui::LabeledWidget( "Up", [&]() {
				if ( ImGui::Vec3Widget( "##up", lightUp, "Light up vector (controls vertical spread)" ) ) {
					changed = true;
				}
			});

			ImGui::LabeledWidget( "Explicit Bounds", [&]() {
				if ( ImGui::Checkbox( "##explicitStartEnd", &explicitStartEnd ) ) {
					if ( explicitStartEnd ) {
						lightStart = lightTarget * 0.25f;
						lightEnd = lightTarget;
					}
					changed = true;
				}
				ImGui::AddTooltip( "Use explicit start and end points instead of automatic" );
			});

			if ( explicitStartEnd ) {
				ImGui::LabeledWidget( "Start", [&]() {
					if ( ImGui::Vec3Widget( "##start", lightStart, "Light frustum start point" ) ) {
						changed = true;
					}
				});

				ImGui::LabeledWidget( "End", [&]() {
					if ( ImGui::Vec3Widget( "##end", lightEnd, "Light frustum end point" ) ) {
						changed = true;
					}
				});
			}
		});

		if ( changed ) {
			ApplyLightChanges();
		}
	}
}

void hcLightEditor::DrawOptionsSection( void ) {
	if ( ImGui::CollapsingHeader( "Rendering Options" ) ) {
		bool changed = false;

		ImGui::PropertyTable( "RenderingOptions", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Shadows", [&]() {
				bool castShadows = !noShadows;
				if ( ImGui::Checkbox( "##castShadows", &castShadows ) ) {
					noShadows = !castShadows;
					changed = true;
				}
				ImGui::AddTooltip( "Enable shadow casting for this light" );
			});

			ImGui::LabeledWidget( "Specular", [&]() {
				bool castSpecular = !noSpecular;
				if ( ImGui::Checkbox( "##castSpecular", &castSpecular ) ) {
					noSpecular = !castSpecular;
					changed = true;
				}
				ImGui::AddTooltip( "Enable specular highlights for this light" );
			});

			ImGui::LabeledWidget( "Diffuse", [&]() {
				bool castDiffuse = !noDiffuse;
				if ( ImGui::Checkbox( "##castDiffuse", &castDiffuse ) ) {
					noDiffuse = !castDiffuse;
					changed = true;
				}
				ImGui::AddTooltip( "Enable diffuse lighting for this light" );
			});
		});

		if ( changed ) {
			ApplyLightChanges();
		}
	}
}

void hcLightEditor::DrawInfoSection( void ) {
	if ( ImGui::CollapsingHeader( "Info" ) ) {
		if ( selectedLight && gameEdit ) {
			const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( selectedLight );
			if ( spawnArgs ) {
				ImGui::PropertyTable( "InfoProperties", propertyTableWidth, [&]() {
					ImGui::LabeledWidget( "Name", [&]() {
						ImGui::TextUnformatted( spawnArgs->GetString( "name", "<unnamed>" ) );
					});

					ImGui::LabeledWidget( "Classname", [&]() {
						ImGui::TextUnformatted( spawnArgs->GetString( "classname", "light" ) );
					});

					ImGui::LabeledWidget( "Type", [&]() {
						ImGui::TextUnformatted( isPointLight ?
							(isParallel ? "Parallel (Sun)" : "Point Light") : "Projected Light" );
					});
				});

				if ( ImGui::TreeNode( "Entity Spawn Args" ) ) {
					ImGui::PropertyTable( "SpawnArgsTable", 150.0f, [&]() {
						for ( int i = 0; i < spawnArgs->GetNumKeyVals(); i++ ) {
							const idKeyValue* kv = spawnArgs->GetKeyVal( i );
							if ( kv ) {
								ImGui::LabeledWidget( kv->GetKey().c_str(), [&]() {
									ImGui::TextUnformatted( kv->GetValue().c_str() );
								});
							}
						}
					});
					ImGui::TreePop();
				}
			}
		}
	}
}

void hcLightEditor::DrawTextureBrowserListTab( void ) {
	RebuildFilteredTextureList();

	ImGui::TextDisabled( "Showing %d of %d textures", filteredTextureIndices.Num(), textureNames.Num() );

	ImGui::BeginChild( "TextureList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders );

	int totalItems = filteredTextureIndices.Num();
	if ( totalItems == 0 ) {
		ImGui::TextDisabled( "No textures match the filter" );
		ImGui::EndChild();
		return;
	}

	// Handle scroll-to-selection
	if ( textureBrowserScrollToIdx >= 0 ) {
		for ( int fi = 0; fi < filteredTextureIndices.Num(); fi++ ) {
			if ( filteredTextureIndices[fi] == textureBrowserScrollToIdx ) {
				float scrollY = fi * ImGui::GetTextLineHeightWithSpacing();
				ImGui::SetScrollY( scrollY );
				break;
			}
		}
		textureBrowserScrollToIdx = -1;
	}

	// Use clipper for virtualized rendering, only render what is visible
	ImGuiListClipper clipper;
	clipper.Begin( totalItems );

	while ( clipper.Step() ) {
		for ( int fi = clipper.DisplayStart; fi < clipper.DisplayEnd; fi++ ) {
			int i = filteredTextureIndices[fi];
			bool isSelected = (currentTextureIndex == i + 1);

			if ( ImGui::Selectable( textureNames[i].c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
				currentTextureIndex = i + 1;
				lightTexture = textureNames[i];
				ApplyLightChanges();
				if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
					showTextureBrowser = false;
					ImGui::CloseCurrentPopup();
				}
			}
		}
	}

	clipper.End();

	ImGui::EndChild();
}

void hcLightEditor::DrawTextureBrowserVisualTab( void ) {
	RebuildFilteredTextureList();

	// Size slider and count display
	ImGui::Text( "Size:" );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( 100 );
	ImGui::SliderFloat( "##ThumbnailSize", &textureThumbnailSize, 32.0f, 128.0f, "%.0f" );

	ImGui::SameLine();
	ImGui::TextDisabled( "Showing %d of %d textures", filteredTextureIndices.Num(), textureNames.Num() );

	ImGui::BeginChild( "TextureGrid", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders );

	int totalItems = filteredTextureIndices.Num();
	if ( totalItems == 0 ) {
		ImGui::TextDisabled( "No textures match the filter" );
		ImGui::EndChild();
		return;
	}

	// Calculate grid layout
	float windowWidth = ImGui::GetContentRegionAvail().x;
	float itemWidth = textureThumbnailSize + ImGui::GetStyle().ItemSpacing.x;
	int columns = Max( 1, (int)(windowWidth / itemWidth) );
	int totalRows = (totalItems + columns - 1) / columns;
	float rowHeight = textureThumbnailSize + ImGui::GetStyle().ItemSpacing.y;

	// Handle scroll-to-selection
	if ( textureBrowserScrollToIdx >= 0 ) {
		for ( int fi = 0; fi < filteredTextureIndices.Num(); fi++ ) {
			if ( filteredTextureIndices[fi] == textureBrowserScrollToIdx ) {
				int targetRow = fi / columns;
				float scrollY = targetRow * rowHeight;
				ImGui::SetScrollY( scrollY );
				break;
			}
		}
		textureBrowserScrollToIdx = -1;
	}

	// Use clipper for virtualized rendering
	ImGuiListClipper clipper;
	clipper.Begin( totalRows, rowHeight );

	while ( clipper.Step() ) {
		for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ ) {
			int startIdx = row * columns;
			int endIdx = Min( startIdx + columns, totalItems );

			for ( int col = 0; col < (endIdx - startIdx); col++ ) {
				int filteredIdx = startIdx + col;
				int i = filteredTextureIndices[filteredIdx];

				bool isSelected = (currentTextureIndex == i + 1);

				// Get material and its editor image
				const idMaterial* mat = declManager->FindMaterial( textureNames[i].c_str(), false );
				idImage* image = nullptr;
				if ( mat ) {
					image = mat->GetEditorImage();
				}

				ImGui::PushID( i );

				if ( col > 0 ) {
					ImGui::SameLine();
				}

				ImGui::BeginGroup();

				// Draw image or placeholder
				ImVec2 imageSize( textureThumbnailSize, textureThumbnailSize );
				bool clicked = false;

				if ( image && image->texnum != idImage::TEXTURE_NOT_LOADED ) {
					ImTextureID texId = (ImTextureID)(intptr_t)image->texnum;

					if ( isSelected ) {
						ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
					}

					clicked = ImGui::ImageButton( "##teximg", texId, imageSize, ImVec2(0,0), ImVec2(1,1) );

					if ( isSelected ) {
						ImGui::PopStyleColor();
					}
				} else {
					if ( isSelected ) {
						ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
					}

					clicked = ImGui::Button( "##placeholder", imageSize );

					if ( isSelected ) {
						ImGui::PopStyleColor();
					}

					// Draw centered placeholder text
					ImVec2 pos = ImGui::GetItemRectMin();
					ImVec2 size = ImGui::GetItemRectSize();
					ImDrawList* drawList = ImGui::GetWindowDrawList();

					const char* placeholderText = (textureThumbnailSize >= 64.0f) ? "No Preview" : "N/A";
					ImVec2 textSize = ImGui::CalcTextSize( placeholderText );
					ImVec2 textPos(
						pos.x + (size.x - textSize.x) * 0.5f,
						pos.y + (size.y - textSize.y) * 0.5f
					);
					drawList->AddText( textPos, IM_COL32(128,128,128,255), placeholderText );
				}

				if ( clicked ) {
					currentTextureIndex = i + 1;
					lightTexture = textureNames[i];
					ApplyLightChanges();
					if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
						showTextureBrowser = false;
						ImGui::CloseCurrentPopup();
					}
				}

				// Tooltip with the texture name
				if ( ImGui::IsItemHovered() ) {
					ImGui::SetTooltip( "%s", textureNames[i].c_str() );
				}

				// Draw selection highlight
				if ( isSelected ) {
					ImVec2 min = ImGui::GetItemRectMin();
					ImVec2 max = ImGui::GetItemRectMax();
					ImGui::GetWindowDrawList()->AddRect( min, max, IM_COL32(255, 200, 0, 255), 0.0f, 0, 2.0f );
				}

				ImGui::EndGroup();

				ImGui::PopID();
			}
		}
	}

	clipper.End();

	ImGui::EndChild();
}

void hcLightEditor::DrawTextureBrowser( void ) {
	if ( !showTextureBrowser ) {
		return;
	}

	ImGui::OpenPopup( "Light Texture Browser###TextureBrowserPopup" );

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
	ImGui::SetNextWindowSize( ImVec2(600, 500), ImGuiCond_Appearing );

	if ( ImGui::BeginPopupModal( "Light Texture Browser###TextureBrowserPopup", &showTextureBrowser, ImGuiWindowFlags_None ) ) {
		// Filter input
		ImGui::Text( "Filter:" );
		ImGui::SameLine();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		if ( ImGui::InputText( "##TextureFilter", textureFilter, sizeof(textureFilter) ) ) {
			textureBrowserScrollToIdx = -1;
		}
		ImGui::AddTooltip( "Type to filter textures" );

		ImGui::Separator();

		// Tab bar
		if ( ImGui::BeginTabBar( "TextureBrowserTabs" ) ) {
			if ( ImGui::BeginTabItem( "List" ) ) {
				textureBrowserTab = 0;
				DrawTextureBrowserListTab();
				ImGui::EndTabItem();
			}

			if ( ImGui::BeginTabItem( "Gallery" ) ) {
				textureBrowserTab = 1;
				DrawTextureBrowserVisualTab();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		// Footer
		ImGui::Text( "Current: %s", currentTextureIndex == 0 ? "<None>" : lightTexture.c_str() );

		if ( ImGui::Button( "None" ) ) {
			currentTextureIndex = 0;
			lightTexture = "";
			ApplyLightChanges();
		}
		ImGui::AddTooltip( "Clear texture selection" );

		ImGui::SameLine();

		if ( ImGui::Button( "Cancel" ) ) {
			showTextureBrowser = false;
			currentTextureIndex = 0;
			lightTexture = "";
			ApplyLightChanges();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if ( ImGui::Button( "OK" ) ) {
			showTextureBrowser = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void hcLightEditor::Draw( void ) {
	if ( !visible ) {
		return;
	}

	ImGuizmo::BeginFrame();

	if ( !initialized ) {
		initialized = true;
	}

	// Check for newly selected light
	CheckSelectedLight();

	const float fontSize = ImGui::GetFontSize();
	ImVec2 minSize( fontSize * 28, fontSize * 25 );
	ImVec2 maxSize( ImGui::GetMainViewport()->WorkSize );
	ImGui::SetNextWindowSizeConstraints( minSize, maxSize );

	ImGuiWindowFlags winFlags = ImGuiWindowFlags_None;

	if ( !ImGui::Begin( "Light Editor###LightEditorWindow", &visible, winFlags ) ) {
		ImGui::End();
		return;
	}

	if ( selectedLight == nullptr ) {
		ImGui::TextDisabled( "No light selected." );
		ImGui::TextDisabled( "Set g_editEntityMode to 1 and click any light to select." );
		ImGui::End();

		if ( !visible ) {
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_LightEditor );
		}

		return;
	}

	// Draw all sections
	DrawGizmoControls();
	DrawLightTypeSection();
	DrawTransformSection();
	DrawColorSection();
	DrawPointLightSection();
	DrawProjectedLightSection();
	DrawOptionsSection();
	DrawInfoSection();

	ImGui::End();

	// Draw the gizmo overlay
	DrawGizmo();

	// Draw modal dialogs
	DrawTextureBrowser();

	// Close window if requested
	if ( !visible ) {
		D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_LightEditor );
	}
}

void Com_DrawImGuiLightEditor( void ) {
	if ( lightEditor ) {
		lightEditor->Draw();
	}
}

/*
================
Com_OpenCloseImGuiLightEditor

Open/Close handler
================
*/
void Com_OpenCloseImGuiLightEditor( bool open ) {
	if ( !lightEditor ) {
		lightEditor = new hcLightEditor();
	}

	if ( open ) {
		lightEditor->Init( nullptr );
	} else {
		lightEditor->Shutdown();
	}
}

/*
================
Com_ImGuiLightEditor_f

Console command handler
================
*/
void Com_ImGuiLightEditor_f( const idCmdArgs& args ) {
	bool menuOpen = (D3::ImGuiHooks::GetOpenWindowsMask() & D3::ImGuiHooks::D3_ImGuiWin_LightEditor) != 0;
	if ( !menuOpen ) {
		D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_LightEditor );
	} else {
		if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_AnyWindow ) ) {
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_LightEditor );
		} else {
			ImGui::SetNextWindowFocus();
		}
	}
}

#else // IMGUI_DISABLE - stub implementations

#include "framework/Common.h"
#include "LightEditor.h"

hcLightEditor* lightEditor = nullptr;

hcLightEditor::hcLightEditor( void ) {}
hcLightEditor::~hcLightEditor( void ) {}
void hcLightEditor::Init( const idDict* spawnArgs ) {}
void hcLightEditor::Shutdown( void ) {}
void hcLightEditor::Draw( void ) {}
bool hcLightEditor::IsVisible( void ) const { return false; }
void hcLightEditor::SetVisible( bool visible ) {}

void Com_DrawImGuiLightEditor( void ) {}
void Com_OpenCloseImGuiLightEditor( bool open ) {}

void Com_ImGuiLightEditor_f( const idCmdArgs& args ) {
	common->Warning( "This editor requires imgui to be enabled" );
}

#endif // IMGUI_DISABLE
