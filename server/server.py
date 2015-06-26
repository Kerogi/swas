import tornado
import tornado.ioloop
import tornado.web
import os, uuid
 
__UPLOADS__ = "uploads/"
 
class Userform(tornado.web.RequestHandler):
    def get(self):
        items = []
        for filename in os.listdir(__UPLOADS__):
            items.append(filename)
        self.render("index.html", items=items)
 
 
class Upload(tornado.web.RequestHandler):
    def post(self):
        if self.request.files['filearg']:
            fileinfo = self.request.files['filearg'][0]
            print "Receiving file:", fileinfo['filename']
            fname = fileinfo['filename']
            cname = str(uuid.uuid4()) + '.' +fname 
            fh = open(__UPLOADS__ + cname, 'w')
            fh.write(fileinfo['body'])
            #self.finish(cname + " is uploaded!! Check %s folder" %__UPLOADS__)
            self.redirect("/")
 
 
application = tornado.web.Application([
        (r"/", Userform),
        (r"/upload", Upload),
        ], debug=True)
 
 
if __name__ == "__main__":
    print 'Server started at port 9000'
    upload_dir = os.path.join(os.getcwd(), __UPLOADS__)
    if not os.path.exists(upload_dir):
        os.mkdir(upload_dir)
    application.listen(9000)
    tornado.ioloop.IOLoop.instance().start()
    