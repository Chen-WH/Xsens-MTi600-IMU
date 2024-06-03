clc; clear;
% 创建串口对象
s = serialport('COM3', 460800); % 替换 'COM3' 和 460800 为你的设置

% 配置超时时间和终止符
configureTerminator(s, "LF"); % 根据需要设置终止符
s.Timeout = 3; % 设置超时时间为3秒

% 进入测量模式
measureCommand = [hex2dec('FA'), hex2dec('FF'), hex2dec('10'), hex2dec('00'), hex2dec('F1')];
write(s, measureCommand, "uint8");

% 接收测量数据
for num = 1:20
    if s.NumBytesAvailable > 0
        data = read(s, s.NumBytesAvailable, "uint8");
        disp('收到数据:');
        disp(data);
        % 解析数据（根据协议格式）
    end
    pause(0.0025); % 避免过度占用CPU
end

% 清除串口对象
clear s;