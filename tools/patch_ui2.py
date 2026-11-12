p = r"c:\Users\SuperUsuario\Documents\Test\VTBot\src\dll\ui.cpp"
s = open(p, encoding="utf-8").read()
s = s.replace('BeginTable("##spells", 6,', 'BeginTable("##spells", 7,')
s = s.replace('''        ImGui::TableSetupColumn("Rango", ImGuiTableColumnFlags_WidthFixed, 44);
        ImGui::TableHeadersRow();''', '''        ImGui::TableSetupColumn("Rango", ImGuiTableColumnFlags_WidthFixed, 44);
        ImGui::TableSetupColumn("ID / CD", ImGuiTableColumnFlags_WidthFixed, 78);
        ImGui::TableHeadersRow();''')
s = s.replace('''            ImGui::TableSetColumnIndex(5); IntCell("##rng", r.range, 1, 8);
''', '''            ImGui::TableSetColumnIndex(5); IntCell("##rng", r.range, 1, 8);
            ImGui::TableSetColumnIndex(6);
            if (!r.enabled || r.words.Get().empty() || !s.ok || (size_t)i >= s.ruleId.size()) {
                ImGui::TextDisabled("-");
            } else if (s.ruleId[i] == 0) {
                ImGui::TextDisabled("buscando...");
            } else if (s.ruleCdMs[i] > 0) {
                ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1), "%d  %.1fs", s.ruleId[i], s.ruleCdMs[i] / 1000.0f);
            } else {
                ImGui::TextColored(kAccent, "%d  listo", s.ruleId[i]);
            }
''')
s = s.replace('"Min = monstruos minimos a distancia <= Rango para lanzar el hechizo (usa 1 para ataques a un objetivo; 2-3+ para hechizos de area)."',
              '"Min = monstruos minimos a distancia <= Rango (1 para ataques a un objetivo; 2-3+ para hechizos de area). \\"CD ms\\" solo se usa mientras no se conozca el ID del hechizo; con ID se usa el cooldown real del juego."')
open(p, "w", encoding="utf-8").write(s)
print("ok", s.count("ID / CD"))
