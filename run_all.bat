@echo off
setlocal enabledelayedexpansion
cls
echo ===============================================================================
echo   CHUONG TRINH CHAY TOAN BO BAI TAP LON: DE 8 - FLOW SHOP HOAN VI (PFSP)
echo   Day chuyen bao duong toa xe - Thu muc: D:\pfsp (2)\pfsp
echo ===============================================================================
echo.

cd /d "%~dp0"

echo [BUOC 1/4] Kiem chung vi du nho de bai (data\vidu\vidu.txt: 5 cong viec x 3 may)
echo Muc tieu: Toi uu duy nhat Cmax = 27 (hoan vi 5 1 2 4 3), NEH cho Cmax = 28.
echo -------------------------------------------------------------------------------
.\pfsp.exe data\vidu\vidu.txt --algo all
echo.

echo [BUOC 2/4] Kiem chung quy tac Johnson cho m = 2 (data\tusinh\rand_6x2.txt: 6 x 2)
echo Muc tieu: Giai dung toi uu O(n log n).
echo -------------------------------------------------------------------------------
.\pfsp.exe data\tusinh\rand_6x2.txt --algo johnson
echo.

echo [BUOC 3/4] Kiem thu tren Taillard ta001 (data\chuan\ta001.txt: 20 cong viec x 5 may)
echo Muc tieu: So sanh cac thuat toan voi BKS = 1278.
echo -------------------------------------------------------------------------------
.\pfsp.exe data\chuan\ta001.txt 800 42
echo.

echo [BUOC 4/4] Tham dinh toan bo 10 instance Taillard bang checker.exe
echo -------------------------------------------------------------------------------
python -c "import subprocess, os; (os.remove('results_20_5.txt') if os.path.exists('results_20_5.txt') else None); [subprocess.run(f'.\\pfsp.exe data\\chuan\\tai20_5.txt --inst {i} --algo ig --time 200 --seed 42 --append results_20_5.txt', shell=True) for i in range(1, 11)]"
.\checker.exe data\chuan\tai20_5.txt results_20_5.txt
echo.

echo ===============================================================================
echo   HOAN TAT TOAN BO CAC BUOC THUC NGHIEM!
echo ===============================================================================
pause
