import sys, signal, time
import smtplib
from os.path import basename
from email.mime.application import MIMEApplication
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText

end = False
def signal_handling(signum, frame):           
    global end                         
    end = True 

def input_args1():
	print("Message:")
	mail_to = input("\tTo (mail): ")
	mail_from = input("\tFrom (mail): ")
	password = input("\tPassword: ")
	return mail_to, mail_from, password

def input_args2():
	time_interval = int(input("Time interval: "))
	mail_text = input("Message text: ")
	filename = input("Filename (0 - without file): ")
	return time_interval, mail_text, filename


def form_message(mail_to, mail_from, mail_text):
	msg = MIMEMultipart()
	msg['From'] = mail_from
	msg['To'] = mail_to
	msg['Subject'] = "Test message"
	msg.attach(MIMEText(mail_text, 'plain'))
	return msg


def add_file(msg, filename):
	if (filename != '0'):
		with open(filename, 'rb') as file:
			part = MIMEApplication(file.read(), Name=basename(filename))
		part['Content-Disposition'] = 'attachment; filename="%s"' % basename(filename)
		msg.attach(part)

	return msg


def send_message(smtphost, login, password, msg):
	server = smtplib.SMTP(smtphost[0], smtphost[1]) 				#открываем подключение
	server.starttls() 												#включаем шифрование
	server.login(login, password)	
	res = server.sendmail(msg['From'], msg['To'], msg.as_string())
	server.quit() 													#закрываем подключение


def main():
	mail_to, mail_from, password = input_args1()
	time_interval, mail_text, filename = input_args2()

	msg = form_message(mail_to, mail_from, mail_text)
	msg = add_file(msg, filename)

	smtphost = ["smtp.mail.ru", 25]

	signal.signal(signal.SIGINT,signal_handling)
	while (not end):
		send_message(smtphost, mail_from, password, msg);
		print("Message sent to %s!" % (mail_to))
		time.sleep(time_interval)


if __name__ == '__main__':
	main()