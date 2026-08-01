from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from parser import analyze_html, analyze_html_file


class HtmlUmgParserTest(unittest.TestCase):
    def test_explicit_data_umg_contract(self) -> None:
        html = """
        <button
          data-umg-type="Button"
          data-umg-name="BtnConfirm"
          data-umg-anchor="bottom-right"
          data-umg-x="1700"
          data-umg-y="1000"
          data-umg-w="180"
          data-umg-h="48"
          data-umg-visibility="collapsed"
          data-umg-z="7"
          data-umg-color="#f5ebd7"
          data-umg-bg="#a05a46"
          data-umg-font-size="24"
        >确认</button>
        """
        tree = analyze_html(
            html,
            blueprint_name="WBP_Test",
            output_path="/Game/UI/Test",
        )
        button = tree["widgets"][0]["children"][0]
        self.assertEqual(tree["status"], "success")
        self.assertEqual(button["name"], "BtnConfirm")
        self.assertEqual(button["visibility"], "collapsed")
        self.assertEqual(button["z_order"], 7)
        self.assertEqual(button["style"]["color"], "#f5ebd7")
        self.assertEqual(button["style"]["background_color"], "#a05a46")
        self.assertEqual(button["style"]["font_size"], 24)

    def test_data_umg_brush_passes_through(self) -> None:
        html = """
        <div
          data-umg-type="Image"
          data-umg-name="PanelBackground"
          data-umg-anchor="top-left"
          data-umg-x="420"
          data-umg-y="180"
          data-umg-w="1080"
          data-umg-h="720"
          data-umg-brush="/Game/UI/Texture/T_UI_Background_SettingsPanel_01"
        ></div>
        <button
          data-umg-type="Button"
          data-umg-name="BtnApply"
          data-umg-anchor="top-left"
          data-umg-x="680"
          data-umg-y="655"
          data-umg-w="220"
          data-umg-h="64"
          data-umg-color="#efe6d0"
          data-umg-brush="/Game/UI/Texture/T_UI_Frame_ButtonBackground_01"
        >应用</button>
        """
        tree = analyze_html(
            html,
            blueprint_name="WBP_Test",
            output_path="/Game/UI/Test",
        )
        children = {c["name"]: c for c in tree["widgets"][0]["children"]}
        self.assertEqual(tree["status"], "success")
        self.assertEqual(
            children["PanelBackground"]["style"]["brush"],
            "/Game/UI/Texture/T_UI_Background_SettingsPanel_01",
        )
        self.assertEqual(
            children["BtnApply"]["style"]["brush"],
            "/Game/UI/Texture/T_UI_Frame_ButtonBackground_01",
        )

    def test_main_hud_contract(self) -> None:
        html_path = (
            Path(__file__).resolve().parents[4]
            / "Art"
            / "workspace"
            / "hud-html-mockup"
            / "index.html"
        )
        tree = analyze_html_file(str(html_path))

        widgets: dict[str, dict] = {}

        def collect(widget: dict) -> None:
            widgets[widget["name"]] = widget
            for child in widget.get("children", []):
                collect(child)

        for root in tree["widgets"]:
            collect(root)

        self.assertEqual(tree["status"], "success")
        self.assertEqual(widgets["BtnResourceToggle"]["text"], "收起 ▾")
        self.assertEqual(widgets["ContextDockRoot"]["anchors"], "bottom-left")
        self.assertEqual(widgets["BtnExpand"]["visibility"], "collapsed")
        self.assertEqual(widgets["BtnRecruitLingZhiFu"]["visibility"], "collapsed")
        self.assertEqual(widgets["BtnForceEndTurn"]["visibility"], "collapsed")
        self.assertEqual(widgets["ForceEndConfirmRoot"]["visibility"], "collapsed")
        self.assertEqual(widgets["ForceEndConfirmRoot"]["z_order"], 100)
        self.assertEqual(widgets["RootCanvas"]["visibility"], "self-hit-test-invisible")
        self.assertIn("BtnForceEndConfirmOk", widgets)
        self.assertIn("BtnForceEndConfirmCancel", widgets)

    def test_malformed_self_closing_data_umg_errors(self) -> None:
        html = '<div class="hud"></div><img data-umg-type="Image" data-umg-name="Bg" />'
        tree = analyze_html(html)
        self.assertEqual(tree["status"], "error")
        self.assertIn("data-umg-type", tree["message"])

    def test_data_umg_missing_required_attrs_errors(self) -> None:
        html = '<div data-umg-type="Button">确认</div>'
        tree = analyze_html(html)
        self.assertEqual(tree["status"], "error")
        self.assertIn("必填属性", tree["message"])


if __name__ == "__main__":
    unittest.main()
