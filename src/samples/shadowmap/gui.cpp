#include "shadowmap_render.h"

#include "../../render/render_gui.h"


void SimpleShadowmapRender::DoImGUI()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    ImGui::ColorEdit3("Meshes base color", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat3("Light source position", m_light.cam.pos.M, -10.f, 10.f);

    ShadowmapChoiceGUI();
    AAChoiceGui();

    ImGui::NewLine();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}

void SimpleShadowmapRender::ShadowmapChoiceGUI()
{
  const char *items[eShTechMax]  = { "Simple", "VSM", "PCF" };
  static const char *currentItem = items[currentShadowmapTechnique];

  ImGuiStyle &style = ImGui::GetStyle();
  float w           = ImGui::CalcItemWidth();
  float buttonSize  = ImGui::GetFrameHeight();
  float spacing     = style.ItemInnerSpacing.x;
  ImGui::PushItemWidth(w - 2.0f * spacing - 2.0f * buttonSize);
  if (ImGui::BeginCombo("##shadow technique", currentItem, ImGuiComboFlags_NoArrowButton))
  {
    for (int i = 0; i < IM_ARRAYSIZE(items); i++)
    {
      bool selected = (currentItem == items[i]);
      if (ImGui::Selectable(items[i], selected))
      {
        currentItem               = items[i];
        currentShadowmapTechnique = (ShadowmapTechnique)i;
      }
      if (selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();
  ImGui::SameLine(0, style.ItemInnerSpacing.x);
  ImGui::Text("Shadow technique");
}

void SimpleShadowmapRender::AAChoiceGui()
{
  // @TODO: taa
  const char *items[]   = { "None", "SSAAx2", "SSAAx4", "SSAAx8", "MSAAx2", "MSAAx4", "MSAAx8" };
  auto state_to_item = [items, this]() {
    if (currentAAScale == eNone)
      return items[0];
    int offset = currentAAScale == e2x ? 0 : (currentAAScale == e4x ? 1 : 2);
    return items[1 + (currentAATechnique == eSsaa ? 0 : 1) * 3 + offset];
  };
  auto id_to_scale = [this](int id) {
    if (id == 0)
      return currentAAScale;
    int scaleId = id % 3;
    return scaleId == 0 ? e8x : (scaleId == 1 ? e2x : e4x);
  };
  auto id_to_tech = [](int id) {
    return id < 1 ? eNone : (id < 4 ? eSsaa : eMsaa);
  };

  static const char *currentItem = state_to_item();

  ImGuiStyle &style = ImGui::GetStyle();
  float w           = ImGui::CalcItemWidth();
  float buttonSize  = ImGui::GetFrameHeight();
  float spacing     = style.ItemInnerSpacing.x;

  ImGui::PushItemWidth(w - 2.0f * spacing - 2.0f * buttonSize);
  AAScale oldAAScale    = currentAAScale;
  if (ImGui::BeginCombo("##AA technique", currentItem, ImGuiComboFlags_NoArrowButton))
  {
    for (int i = 0; i < IM_ARRAYSIZE(items); i++)
    {
      bool selected = (currentItem == items[i]);
      if (ImGui::Selectable(items[i], selected))
      {
        currentItem        = items[i];
        currentAATechnique = id_to_tech(i);
        currentAAScale     = id_to_scale(i);
      }
      if (selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();

  ImGui::SameLine(0, style.ItemInnerSpacing.x);
  ImGui::Text("Anti-aliasing technique");

  if (currentAAScale != oldAAScale)
    RecreateAATexOnScaleChange();
}
