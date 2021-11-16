import smtplib
import sys
from os.path import isfile, join, basename, splitext
from os import listdir
from email.mime.application import MIMEApplication
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText

def input_args1():
	print("Message:")
	mail_to = input("\tTo (mail): ")
	mail_from = input("\tFrom (mail): ")
	password = input("\tPassword: ")
	return mail_to, mail_from, password

def input_args2():
	time = input("Time interval: ")
	mail_text = input("Message text: ")
	filename = input("Filename (0 - without file): ")

	return time, mail_text, filename


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
	#mail_to, mail_from, password = input_args1()
	mail_to = 'networktest1@mail.ru'
	mail_from = '1platosha@mail.ru'
	password = '21Rfrnec!'

	#time, mail_text, filename = input_args2()
	time = 1
	mail_text = 'qwe'
	filename = 'test.txt'

	msg = form_message(mail_to, mail_from, mail_text)
	msg = add_file(msg, filename)

	smtphost = ["smtp.mail.ru", 25]
	send_message(smtphost, mail_from, password, msg);
	
	print("Message sent to %s!" % (mail_to))


if __name__ == '__main__':
	main()