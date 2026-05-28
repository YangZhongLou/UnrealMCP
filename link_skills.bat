@echo off
set "SKILLS_DIR=SkillHub\.agent\skills"
set "LINK_DIR=.trae\skills"

for /d %%D in ("%SKILLS_DIR%\*") do (
    set "SKILL_NAME=%%~nD"
    setlocal enabledelayedexpansion
    if not exist "%LINK_DIR%\!SKILL_NAME!" (
        mklink /J "%LINK_DIR%\!SKILL_NAME!" "%%D"
        echo Linked: !SKILL_NAME!
    ) else (
        echo Already exists: !SKILL_NAME!
    )
    endlocal
)
